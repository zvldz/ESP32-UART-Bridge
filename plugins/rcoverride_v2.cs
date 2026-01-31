// RcOverridePlugin.cs
// Mission Planner plugin: read "RC <16 pwm values>" from COM port or UDP
// and send RC_OVERRIDE via MAVLink.

using System;
using System.Drawing;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO.Ports;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Windows.Forms;
using MissionPlanner;
using MissionPlanner.Plugin;

namespace RcOverridePlugin
{
    public class RcOverridePlugin : Plugin
    {
        // --- Rate limit (0 = disabled, set to e.g. 50 for 50ms minimum interval) ---
        private const int RATE_LIMIT_MS = 0;

        // --- LED and timeout config ---
        private const int LED_TIMEOUT_MS = 1000;
        private const int DATA_TIMEOUT_MS = 3000;
        private const int BUFFER_MAX_SIZE = 4096;
        private const int PORT_RETRY_INTERVAL_MS = 2000;  // Wait 2s between open attempts (for BT ports)

        // --- Serial config ---
        private const string PORT_DEFAULT = "COM14";
        private const int BAUD = 115200;

        // --- UDP config ---
        private const int UDP_PORT_DEFAULT = 14552;

        // --- Source mode ---
        private enum SourceMode { COM, UDP }
        private SourceMode _sourceMode = SourceMode.COM;

        private string _currentPortName = PORT_DEFAULT;
        private int _udpPort = UDP_PORT_DEFAULT;

        // --- State ---
        private SerialPort _port;
        private UdpClient _udpClient;
        private IPEndPoint _remoteEP;
        private DateTime _lastFrameSent = DateTime.MinValue;
        private DateTime _lastDataTime = DateTime.MinValue;
        private DateTime _lastPacketTime = DateTime.MinValue;
        private DateTime _lastPortOpenAttempt = DateTime.MinValue;
        private bool _running = false;

        // --- UI controls ---
        private Panel _uiPanel;
        private ComboBox _cmbSource;    // COM / UDP
        private ComboBox _cmbPort;      // COM port selection
        private TextBox _txtUdpPort;    // UDP port input
        private CheckBox _chkEnable;
        private Panel _ledPanel;
        private ToolTip _ledToolTip;

        public override string Name => "RC Override";
        public override string Author => "ACh / mod";
        public override string Version => "2.0.3";

        private StringBuilder _buffer = new StringBuilder(256);

        // Diagnostic counters
        private int _linesReceived = 0;
        private int _linesParsed = 0;
        private int _framesSent = 0;
        private DateTime _lastDiagTime = DateTime.MinValue;
        private const int DIAG_INTERVAL_MS = 5000;

        public override bool Init()
        {
            loopratehz = 50;
            return true;
        }

        public override bool Loaded()
        {
            _running = true;
            Console.WriteLine("[RC] Plugin v{0} loaded", Version);
            SetupUiPanel();
            return true;
        }

        public override bool Exit()
        {
            _running = false;
            Console.WriteLine("[RC] Plugin exiting, stats: lines={0}, parsed={1}, sent={2}",
                _linesReceived, _linesParsed, _framesSent);

            try { ClearRcOverride(); } catch { }
            try { ClosePort(); } catch { }
            try { CloseUdp(); } catch { }
            try { RemoveUiPanel(); } catch { }

            return true;
        }

        public override bool Loop()
        {
            if (!_running)
                return true;

            if (_chkEnable != null && !_chkEnable.Checked)
            {
                // Close port when disabled
                ClosePort();
                CloseUdp();
                return true;
            }

            if (!IsVehicleReady())
                return true;

            // Periodic diagnostics
            var diagNow = DateTime.UtcNow;
            if ((diagNow - _lastDiagTime).TotalMilliseconds > DIAG_INTERVAL_MS)
            {
                _lastDiagTime = diagNow;
                Console.WriteLine("[RC] Stats: lines={0}, parsed={1}, sent={2}, port={3}, open={4}",
                    _linesReceived, _linesParsed, _framesSent,
                    _currentPortName,
                    _port != null && _port.IsOpen ? "yes" : "no");
            }

            try
            {
                if (_sourceMode == SourceMode.UDP)
                {
                    EnsureUdp();
                    if (_udpClient == null)
                    {
                        UpdateLed(DateTime.UtcNow);
                        return true;
                    }
                    ProcessUdpData();
                }
                else
                {
                    EnsurePort();
                    if (_port == null || !_port.IsOpen)
                    {
                        UpdateLed(DateTime.UtcNow);
                        return true;
                    }
                    ProcessSerialData();
                }

                // Process buffer for complete lines
                ProcessBuffer();

                // Update LED state
                UpdateLed(DateTime.UtcNow);
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[RC] Loop exception: " + ex.Message);
                if (_sourceMode == SourceMode.UDP)
                    CloseUdp();
                else
                    ClosePort();
            }

            return true;
        }

        // ---------------------------------------------------------------
        // Data processing
        // ---------------------------------------------------------------

        private void ProcessSerialData()
        {
            string incoming = _port.ReadExisting();
            if (!string.IsNullOrEmpty(incoming))
            {
                _buffer.Append(incoming);
                _lastDataTime = DateTime.UtcNow;

                // Prevent buffer overflow
                if (_buffer.Length > BUFFER_MAX_SIZE)
                {
                    Debug.WriteLine("[RC] Buffer overflow, clearing");
                    _buffer.Clear();
                }
            }

            // If no serial data for more than DATA_TIMEOUT_MS, try to recover the port
            if (_port != null && _port.IsOpen && _lastDataTime != DateTime.MinValue)
            {
                var idle = DateTime.UtcNow - _lastDataTime;
                if (idle.TotalMilliseconds > DATA_TIMEOUT_MS)
                {
                    Debug.WriteLine("[RC] No serial data for >" + DATA_TIMEOUT_MS + "ms, closing port to force reopen");
                    ClosePort();
                    _lastDataTime = DateTime.MinValue;
                }
            }
        }

        private void ProcessUdpData()
        {
            try
            {
                while (_udpClient != null && _udpClient.Available > 0)
                {
                    byte[] data = _udpClient.Receive(ref _remoteEP);
                    string incoming = Encoding.ASCII.GetString(data);
                    _buffer.Append(incoming);
                    _lastDataTime = DateTime.UtcNow;

                    // Prevent buffer overflow
                    if (_buffer.Length > BUFFER_MAX_SIZE)
                    {
                        Debug.WriteLine("[RC] Buffer overflow, clearing");
                        _buffer.Clear();
                    }
                }

                // If no UDP data for more than DATA_TIMEOUT_MS, reset state
                if (_lastDataTime != DateTime.MinValue)
                {
                    var idle = DateTime.UtcNow - _lastDataTime;
                    if (idle.TotalMilliseconds > DATA_TIMEOUT_MS)
                    {
                        Debug.WriteLine("[RC] No UDP data for >" + DATA_TIMEOUT_MS + "ms");
                        _lastDataTime = DateTime.MinValue;
                    }
                }
            }
            catch (SocketException)
            {
                // Timeout or no data available - normal
            }
        }

        private void ProcessBuffer()
        {
            string bufStr = _buffer.ToString();
            int newline;
            while ((newline = bufStr.IndexOf('\n')) >= 0)
            {
                string line = bufStr.Substring(0, newline).Trim('\r', '\n', ' ');
                bufStr = bufStr.Substring(newline + 1);
                _linesReceived++;

                ushort[] rc = ParseRcLine(line);
                if (rc != null)
                {
                    _linesParsed++;
                    DateTime now = DateTime.UtcNow;
                    if (RATE_LIMIT_MS == 0 || (now - _lastFrameSent).TotalMilliseconds >= RATE_LIMIT_MS)
                    {
                        _lastFrameSent = now;
                        _lastPacketTime = now;
                        SendRcOverride(rc);
                        _framesSent++;
                    }
                }
            }

            // Update buffer with remaining data
            _buffer.Clear();
            if (bufStr.Length > 0)
                _buffer.Append(bufStr);
        }

        private void SetLedColor(Color color, string tooltip = null)
        {
            if (_ledPanel == null)
                return;

            if (_ledPanel.BackColor != color)
                _ledPanel.BackColor = color;

            if (tooltip != null && _ledToolTip != null)
                _ledToolTip.SetToolTip(_ledPanel, tooltip);
        }

        private void UpdateLed(DateTime now)
        {
            if (_ledPanel == null)
                return;

            // If RC override is disabled, LED is gray and controls unlocked
            if (_chkEnable == null || !_chkEnable.Checked)
            {
                SetLedColor(Color.DarkGray, "Disabled");
                if (_cmbPort != null) _cmbPort.Enabled = true;
                if (_cmbSource != null) _cmbSource.Enabled = true;
                return;
            }

            // Check connection status based on mode
            bool connected = false;
            if (_sourceMode == SourceMode.UDP)
            {
                connected = (_udpClient != null);
            }
            else
            {
                connected = (_port != null && _port.IsOpen);
            }

            if (!connected)
            {
                bool hasTarget = false;
                if (_sourceMode == SourceMode.UDP)
                    hasTarget = true;
                else
                    hasTarget = !string.IsNullOrEmpty(_currentPortName) &&
                                !string.Equals(_currentPortName, "No ports", StringComparison.OrdinalIgnoreCase);

                if (hasTarget)
                {
                    bool blink = (now.Millisecond / 500) % 2 == 0;
                    SetLedColor(blink ? Color.Red : Color.DarkGray, "Connecting...");
                }
                else
                {
                    SetLedColor(Color.DarkGray, "No target selected");
                }
                return;
            }

            // Connected — check vehicle telemetry and data flow
            if (!IsVehicleReady())
                SetLedColor(Color.Orange, "Connected, no vehicle telemetry");
            else if (_lastPacketTime != DateTime.MinValue && (now - _lastPacketTime).TotalMilliseconds < LED_TIMEOUT_MS)
                SetLedColor(Color.LimeGreen, "Connected, data OK");
            else
                SetLedColor(Color.Orange, "Connected, no data from ESP");

            // Lock port/source selection when connected (prevent accidental changes)
            if (_cmbPort != null) _cmbPort.Enabled = !connected;
            if (_cmbSource != null) _cmbSource.Enabled = !connected;
        }

        // ---------------------------------------------------------------
        // Parse "RC 1500 1500 1000 ..."
        // ---------------------------------------------------------------

        private ushort[] ParseRcLine(string line)
        {
            if (string.IsNullOrEmpty(line))
                return null;

            try
            {
                // Example formats we accept:
                // "RC 1500,1500,1000,..."
                // "RC,1500,1500,1000,..."
                // "some noise RC 1500,1500,1000,..."
                // "RC1500,1500,1000,..." (no space)

                // find "RC" even if garbage exists before it
                int start = line.IndexOf("RC");
                if (start < 0)
                    return null;

                // cut everything before RC
                string raw = line.Substring(start);

                // remove CR/LF and whitespace
                raw = raw.Trim();

                // must start with RC
                if (!raw.StartsWith("RC"))
                    return null;

                // remove "RC" and leading comma/space
                string body = raw.Substring(2).TrimStart(',', ' ');

                // find the end of CSV (newline or next RC) - defensive
                int newlineIdx = body.IndexOfAny(new[] { '\r', '\n' });
                if (newlineIdx >= 0)
                    body = body.Substring(0, newlineIdx);

                // now split by comma
                string[] parts = body.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);

                if (parts.Length < 16)
                    return null;

                ushort[] rc = new ushort[16];

                for (int i = 0; i < 16; i++)
                {
                    if (!ushort.TryParse(parts[i].Trim(), out rc[i]))
                        return null;

                    // clamp to valid PWM range for safety
                    if (rc[i] < 800) rc[i] = 800;
                    if (rc[i] > 2200) rc[i] = 2200;
                }

                return rc;
            }
            catch
            {
                return null;
            }
        }

        // ---------------------------------------------------------------
        // MAVLink RC Override
        // ---------------------------------------------------------------

        private void SendRcOverride(ushort[] rc)
        {
            try
            {
                var mav = Host.comPort.MAV;
                if (mav == null)
                    return;

                var msg = new MAVLink.mavlink_rc_channels_override_t
                {
                    target_system = mav.sysid,
                    target_component = mav.compid,

                    chan1_raw = rc[0],
                    chan2_raw = rc[1],
                    chan3_raw = rc[2],
                    chan4_raw = rc[3],
                    chan5_raw = rc[4],
                    chan6_raw = rc[5],
                    chan7_raw = rc[6],
                    chan8_raw = rc[7],
                    chan9_raw = rc[8],
                    chan10_raw = rc[9],
                    chan11_raw = rc[10],
                    chan12_raw = rc[11],
                    chan13_raw = rc[12],
                    chan14_raw = rc[13],
                    chan15_raw = rc[14],
                    chan16_raw = rc[15],
                };

                Host.comPort.sendPacket(msg, msg.target_system, msg.target_component);
            }
            catch { }
        }

        private void ClearRcOverride()
        {
            try
            {
                var mav = Host.comPort.MAV;
                if (mav == null)
                    return;

                var msg = new MAVLink.mavlink_rc_channels_override_t
                {
                    target_system = mav.sysid,
                    target_component = mav.compid,
                    chan1_raw = ushort.MaxValue,
                    chan2_raw = ushort.MaxValue,
                    chan3_raw = ushort.MaxValue,
                    chan4_raw = ushort.MaxValue,
                    chan5_raw = ushort.MaxValue,
                    chan6_raw = ushort.MaxValue,
                    chan7_raw = ushort.MaxValue,
                    chan8_raw = ushort.MaxValue,
                };

                Host.comPort.sendPacket(msg, msg.target_system, msg.target_component);
            }
            catch { }
        }

        // ---------------------------------------------------------------
        // Serial COM port handling
        // ---------------------------------------------------------------

        private void EnsurePort()
        {
            if (_port != null && _port.IsOpen)
                return;

            // Don't try to open port if "No ports" is selected or port name is null/empty
            if (string.IsNullOrEmpty(_currentPortName) || string.Equals(_currentPortName, "No ports", StringComparison.OrdinalIgnoreCase))
                return;

            // Rate limit port open attempts (BT ports need time to establish SPP connection)
            var now = DateTime.UtcNow;
            if ((now - _lastPortOpenAttempt).TotalMilliseconds < PORT_RETRY_INTERVAL_MS)
                return;
            _lastPortOpenAttempt = now;

            try
            {
                ClosePort();

                Console.WriteLine("[RC] Opening port {0} at {1} baud...", _currentPortName, BAUD);
                _port = new SerialPort(_currentPortName, BAUD, Parity.None, 8, StopBits.One);
                _port.ReadTimeout = 5;
                _port.DtrEnable = false;
                // ESP32-S3 USB Serial/JTAG resets on RTS transition during Open().
                // Handshake.RequestToSend makes OS control RTS (no explicit toggle),
                // then switch back to None after Open(). Same trick as MP's espFix.
                _port.Handshake = Handshake.RequestToSend;
                _port.Open();
                _port.Handshake = Handshake.None;

                _lastDataTime = DateTime.UtcNow;

                Console.WriteLine("[RC] Port {0} opened successfully", _currentPortName);
            }
            catch (Exception ex)
            {
                Console.WriteLine("[RC] Failed to open {0}: {1}", _currentPortName, ex.Message);
                ClosePort();
            }
        }

        private void ClosePort()
        {
            try
            {
                if (_port != null)
                {
                    if (_port.IsOpen)
                        _port.Close();
                    _port.Dispose();
                }
            }
            catch { }
            finally
            {
                _port = null;
            }
        }

        // ---------------------------------------------------------------
        // UDP handling
        // ---------------------------------------------------------------

        private void EnsureUdp()
        {
            if (_udpClient != null)
                return;

            try
            {
                _udpClient = new UdpClient(_udpPort);
                _udpClient.Client.ReceiveTimeout = 5;
                _remoteEP = new IPEndPoint(IPAddress.Any, 0);
                _lastDataTime = DateTime.UtcNow;

                Debug.WriteLine("[RC] UDP listening on port " + _udpPort);
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[RC] UDP init failed: " + ex.Message);
                CloseUdp();
            }
        }

        private void CloseUdp()
        {
            try
            {
                _udpClient?.Close();
            }
            catch { }
            finally
            {
                _udpClient = null;
            }
        }

        // ---------------------------------------------------------------
        // Vehicle state check
        // ---------------------------------------------------------------

        private bool IsVehicleReady()
        {
            try
            {
                if (!Host.comPort.BaseStream.IsOpen)
                    return false;
                if (Host.comPort.MAV == null)
                    return false;
                if (!Host.comPort.MAV.cs.connected)
                    return false;

                return true;
            }
            catch
            {
                return false;
            }
        }

        // ---------------------------------------------------------------
        // UI Panel
        // ---------------------------------------------------------------

        private void SetupUiPanel()
        {
            if (Host?.MainForm == null)
                return;

            if (_uiPanel != null)
                return;

            var form = Host.MainForm;

            _uiPanel = new Panel();
            _uiPanel.AutoSize = true;
            _uiPanel.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            _uiPanel.BorderStyle = BorderStyle.None;

            // Source mode selector (COM / UDP)
            _cmbSource = new ComboBox();
            _cmbSource.DropDownStyle = ComboBoxStyle.DropDownList;
            _cmbSource.Width = 55;
            _cmbSource.Margin = new Padding(0);
            _cmbSource.TabStop = false;
            _cmbSource.Items.AddRange(new object[] { "COM", "UDP" });
            _cmbSource.SelectedIndex = 0;
            _cmbSource.Location = new System.Drawing.Point(0, 2);

            _cmbSource.SelectedIndexChanged += (s, e) =>
            {
                try
                {
                    var newMode = _cmbSource.SelectedIndex == 1 ? SourceMode.UDP : SourceMode.COM;
                    if (newMode != _sourceMode)
                    {
                        // Close current connection
                        ClosePort();
                        CloseUdp();
                        _buffer.Clear();
                        _lastDataTime = DateTime.MinValue;

                        _sourceMode = newMode;
                        Debug.WriteLine("[RC] Source mode changed to " + _sourceMode);

                        // Update UI visibility
                        UpdateSourceUi();
                    }
                }
                catch { }
            };

            // COM port selector
            _cmbPort = new ComboBox();
            _cmbPort.DropDownStyle = ComboBoxStyle.DropDownList;
            _cmbPort.Width = 80;
            _cmbPort.Margin = new Padding(0);
            _cmbPort.TabStop = false;
            _cmbPort.Location = new System.Drawing.Point(_cmbSource.Right + 5, 2);

            RefreshPortList();

            // Refresh port list when dropdown is opened
            _cmbPort.DropDown += (s, e) => RefreshPortList();

            _cmbPort.SelectedIndexChanged += (s, e) =>
            {
                try
                {
                    string selected = _cmbPort.SelectedItem as string;
                    // Ignore "No ports" selection
                    if (string.IsNullOrEmpty(selected) || string.Equals(selected, "No ports", StringComparison.OrdinalIgnoreCase))
                        return;

                    if (!string.Equals(selected, _currentPortName, StringComparison.OrdinalIgnoreCase))
                    {
                        _currentPortName = selected;
                        Debug.WriteLine("[RC] Port changed to " + _currentPortName);
                        ClosePort();
                    }
                }
                catch { }
            };

            // UDP port input
            _txtUdpPort = new TextBox();
            _txtUdpPort.Width = 55;
            _txtUdpPort.Text = UDP_PORT_DEFAULT.ToString();
            _txtUdpPort.Margin = new Padding(0);
            _txtUdpPort.TabStop = false;
            _txtUdpPort.Location = new System.Drawing.Point(_cmbSource.Right + 5, 2);
            _txtUdpPort.Visible = false;  // Hidden by default (COM mode)

            _txtUdpPort.TextChanged += (s, e) =>
            {
                try
                {
                    if (int.TryParse(_txtUdpPort.Text, out int port) && port > 0 && port <= 65535)
                    {
                        if (port != _udpPort)
                        {
                            _udpPort = port;
                            Debug.WriteLine("[RC] UDP port changed to " + _udpPort);
                            CloseUdp();
                        }
                    }
                }
                catch { }
            };

            // Enable checkbox
            _chkEnable = new CheckBox();
            _chkEnable.Text = "MAVLINK RC";
            _chkEnable.AutoSize = true;
            _chkEnable.Checked = false;
            _chkEnable.Margin = new Padding(4, 4, 0, 0);
            _chkEnable.TabStop = false;
            _chkEnable.Location = new System.Drawing.Point(_cmbPort.Right + 5, 4);
            _chkEnable.CheckedChanged += (s, e) => {
                if (!_chkEnable.Checked)
                {
                    try { ClearRcOverride(); } catch { }
                    ClosePort();
                    CloseUdp();
                }
                UpdateLed(DateTime.Now);
            };

            // LED indicator panel (red/green/gray)
            _ledPanel = new Panel();
            _ledPanel.Width = 14;
            _ledPanel.Height = 14;
            _ledPanel.Margin = new Padding(4, 4, 0, 0);
            _ledPanel.BorderStyle = BorderStyle.FixedSingle;
            _ledPanel.BackColor = Color.DarkGray;
            _ledPanel.Location = new System.Drawing.Point(_chkEnable.Right + 5, 5);
            _ledToolTip = new ToolTip();
            _ledToolTip.SetToolTip(_ledPanel, "Disabled");

            _uiPanel.Controls.Add(_cmbSource);
            _uiPanel.Controls.Add(_cmbPort);
            _uiPanel.Controls.Add(_txtUdpPort);
            _uiPanel.Controls.Add(_chkEnable);
            _uiPanel.Controls.Add(_ledPanel);

            form.Controls.Add(_uiPanel);
            _uiPanel.Anchor = AnchorStyles.Top | AnchorStyles.Right;

            int y = 5;
            if (form.MainMenuStrip != null)
                y = form.MainMenuStrip.Bottom + 5;

            _uiPanel.Location = new System.Drawing.Point(
                form.ClientSize.Width - _uiPanel.Width - 10,
                y
            );

            form.Resize += (s, e) =>
            {
                if (_uiPanel != null)
                {
                    int yy = 5;
                    if (form.MainMenuStrip != null)
                        yy = form.MainMenuStrip.Bottom + 5;

                    _uiPanel.Location = new System.Drawing.Point(
                        form.ClientSize.Width - _uiPanel.Width - 10,
                        yy
                    );
                }
            };

            _uiPanel.BringToFront();
        }

        private void UpdateSourceUi()
        {
            if (_cmbPort == null || _txtUdpPort == null || _chkEnable == null || _ledPanel == null)
                return;

            if (_sourceMode == SourceMode.UDP)
            {
                _cmbPort.Visible = false;
                _txtUdpPort.Visible = true;
                _chkEnable.Location = new System.Drawing.Point(_txtUdpPort.Right + 5, 4);
            }
            else
            {
                _cmbPort.Visible = true;
                _txtUdpPort.Visible = false;
                _chkEnable.Location = new System.Drawing.Point(_cmbPort.Right + 5, 4);
            }

            _ledPanel.Location = new System.Drawing.Point(_chkEnable.Right + 5, 5);
        }

        private void RefreshPortList()
        {
            if (_cmbPort == null)
                return;

            string[] ports = SerialPort.GetPortNames();
            Array.Sort(ports, StringComparer.OrdinalIgnoreCase);

            // Preserve current selection
            string currentSelection = _currentPortName;

            _cmbPort.Items.Clear();

            if (ports.Length == 0)
            {
                _cmbPort.Items.Add("No ports");
                _cmbPort.SelectedIndex = 0;
                _currentPortName = null;
                return;
            }

            _cmbPort.Items.AddRange(ports);

            int idx = Array.IndexOf(ports, currentSelection);
            if (idx < 0) idx = 0;
            _cmbPort.SelectedIndex = idx;

            // Update _currentPortName to match selected port
            if (_cmbPort.SelectedItem != null)
            {
                string selectedPort = _cmbPort.SelectedItem as string;
                if (!string.IsNullOrEmpty(selectedPort) && !string.Equals(selectedPort, "No ports", StringComparison.OrdinalIgnoreCase))
                {
                    _currentPortName = selectedPort;
                }
            }
        }

        private void RemoveUiPanel()
        {
            try
            {
                if (_uiPanel != null && _uiPanel.Parent != null)
                {
                    _uiPanel.Parent.Controls.Remove(_uiPanel);
                    _uiPanel.Dispose();
                }
            }
            catch { }
            finally
            {
                _uiPanel = null;
                _cmbSource = null;
                _cmbPort = null;
                _txtUdpPort = null;
                _chkEnable = null;
                _ledPanel = null;
            }
        }
    }
}

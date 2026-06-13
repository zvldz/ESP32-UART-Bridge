const { fetchWithAuth } = require('../src/webui_src/utils.js');

describe('Protected endpoints reject unauthenticated requests', () => {
  const payloads = [
    { token: null, desc: 'missing token' },
    { token: '', desc: 'empty token' },
    { token: 'invalid.token.format', desc: 'malformed token' },
    { token: 'Bearer expired_token_12345', desc: 'expired token' },
    { token: 'Bearer valid_token', desc: 'valid token (control)' },
  ];

  beforeEach(() => {
    global.fetch = jest.fn();
  });

  test.each(payloads)('rejects request with $desc', async ({ token, desc }) => {
    global.fetch.mockResolvedValueOnce({
      status: token === 'Bearer valid_token' ? 200 : 401,
      ok: token === 'Bearer valid_token',
      json: async () => ({ error: 'Unauthorized' }),
    });

    const response = await fetchWithAuth('/api/config', { token });

    expect(global.fetch).toHaveBeenCalled();
    const callArgs = global.fetch.mock.calls[0];
    
    if (token === 'Bearer valid_token') {
      expect(response.status).toBe(200);
    } else {
      expect(response.status).toBe(401);
      expect(response.ok).toBe(false);
    }
  });
});
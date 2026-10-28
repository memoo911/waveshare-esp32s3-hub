# Clock Assistant Worker

A small [Cloudflare Worker](https://developers.cloudflare.com/workers/) for the
clock's **Assistant** app. The clock sends a recorded question (WAV, base64)
plus the conversation so far; the Worker asks Google Gemini and returns a
short plain-text answer in the language of the question.

Setup, step by step: see **Setting up the Assistant** in the main
[README](../README.md#setting-up-the-assistant) (Croatian:
[README.hr.md](../README.hr.md#postavljanje-asistenta)).

Short version:

```
cd worker
npx wrangler login
npx wrangler deploy
npx wrangler secret put GEMINI_API_KEY   # your Gemini API key
npx wrangler secret put DEVICE_TOKEN     # a long random string, also in credentials.h
```

No keys or addresses are stored in this folder. The free Workers plan is
plenty for one clock.

| File | What it is |
|---|---|
| `src/index.js` | The Worker: checks the device key, calls Gemini, cleans the text for the clock's font |
| `wrangler.toml` | Name and settings; `GEMINI_MODEL` picks the Gemini model |

API: `POST /ask` with `Authorization: Bearer <DEVICE_TOKEN>` and JSON
`{"audio": "<base64 WAV>", "history": [{"q": "...", "a": "..."}], "lang": "en", "time": "...", "place": "..."}`,
returns `{"question": "...", "answer": "..."}`. `GET /` returns
`{"ok": true}` to check that it's online.

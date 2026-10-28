# Clock Assistant Worker

A small [Cloudflare Worker](https://developers.cloudflare.com/workers/) for the
clock's **AI Assistant** app. The clock sends a recorded question (WAV, base64)
plus the conversation so far; the Worker asks Google Gemini and returns a
short plain-text answer in the language of the question, using Google Search
when the question needs current information.

Setup, step by step: [docs/AI-ASSISTANT.md](../docs/AI-ASSISTANT.md)
(Croatian: [docs/AI-ASSISTANT.hr.md](../docs/AI-ASSISTANT.hr.md)). Your keys
stay in Cloudflare and in your local `src/credentials.h`, never in git.

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

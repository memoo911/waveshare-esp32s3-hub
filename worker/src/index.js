// Cloudflare Worker for the clock's Assistant app.
//
// The clock records a spoken question and POSTs it here as JSON:
//   { "audio": "<base64 WAV>", "history": [{ "q": "...", "a": "..." }, ...],
//     "lang": "en" | "hr", "time": "2026-10-26 21:54 (Monday)", "place": "Zagreb" }
// with the header  Authorization: Bearer <DEVICE_TOKEN>.
// This Worker asks Gemini and replies { "question": "...", "answer": "..." }.
//
// Secrets (set with `npx wrangler secret put ...`, never in a file):
//   GEMINI_API_KEY  your key from Google AI Studio
//   DEVICE_TOKEN    a long random string; the same one goes in the clock's
//                   credentials.h as ASSISTANT_TOKEN
// Optional variable (wrangler.toml): GEMINI_MODEL.

const DEFAULT_MODEL = "gemini-3.8-flash";
const MAX_AUDIO_BASE64 = 2_000_000; // about 60 s at 12 kHz, far above the clock's 15 s
const MAX_HISTORY = 8;
const MAX_ANSWER_CHARS = 600;

const SYSTEM_PROMPT = `You are a voice assistant on a small clock with a 1.8 inch screen.
The user speaks a question; you answer with text that is shown on the screen.

Rules:
- Answer in the same language the question was spoken in. If you can't tell, use the
  language of the conversation so far, otherwise the fallback language given below.
- Keep it short: at most 3 short sentences, about 50 words. Give the answer first,
  no introductions, no follow-up questions unless something is really unclear.
- Plain text only: no markdown, no lists, no asterisks, no emoji, no tables.
- Use metric units and the 24-hour clock.
- This is one ongoing conversation: earlier questions and answers are given as context,
  so follow-ups like "and tomorrow?" refer to them.
- If the audio is silent or you can't understand it, set "answer" to a short sentence
  saying you didn't catch that, in the fallback language.
- You have no internet access. For live data (weather, news, prices) say briefly that
  you can't check it right now.

Return JSON: "question" is the user's question as you understood it (a transcript),
"answer" is your reply.`;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method === "GET" && url.pathname === "/") {
      return json({ ok: true, service: "clock assistant" });
    }
    if (request.method !== "POST" || url.pathname !== "/ask") {
      return json({ error: "not found" }, 404);
    }
    if (!env.DEVICE_TOKEN || !env.GEMINI_API_KEY) {
      return json({ error: "worker not configured (secrets missing)" }, 500);
    }
    const auth = request.headers.get("Authorization") || "";
    if (!(await sameText(auth, `Bearer ${env.DEVICE_TOKEN}`))) {
      return json({ error: "unauthorized" }, 401);
    }

    let body;
    try {
      body = await request.json();
    } catch {
      return json({ error: "bad json" }, 400);
    }
    const audio = typeof body.audio === "string" ? body.audio : "";
    if (audio.length < 100 || audio.length > MAX_AUDIO_BASE64) {
      return json({ error: "missing or too long audio" }, 400);
    }
    const lang = body.lang === "hr" ? "Croatian" : "English";
    const time = typeof body.time === "string" ? body.time.slice(0, 60) : "";
    const place = typeof body.place === "string" ? body.place.slice(0, 60) : "";

    const contents = [];
    const history = Array.isArray(body.history) ? body.history.slice(-MAX_HISTORY) : [];
    for (const turn of history) {
      if (!turn || typeof turn.q !== "string" || typeof turn.a !== "string") continue;
      contents.push({ role: "user", parts: [{ text: turn.q.slice(0, 500) }] });
      contents.push({ role: "model", parts: [{ text: turn.a.slice(0, 1000) }] });
    }
    contents.push({
      role: "user",
      parts: [
        { inlineData: { mimeType: "audio/wav", data: audio } },
        { text: "This recording is my spoken question. Answer it." },
      ],
    });

    let context = `Fallback language: ${lang}.`;
    if (time) context += ` Local date and time at the clock: ${time}.`;
    if (place) context += ` The clock is in or near: ${place}.`;

    const model = env.GEMINI_MODEL || DEFAULT_MODEL;
    const gemini = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models/${encodeURIComponent(model)}:generateContent`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json", "x-goog-api-key": env.GEMINI_API_KEY },
        body: JSON.stringify({
          systemInstruction: { parts: [{ text: `${SYSTEM_PROMPT}\n\n${context}` }] },
          contents,
          generationConfig: {
            temperature: 0.5,
            responseMimeType: "application/json",
            responseSchema: {
              type: "OBJECT",
              properties: {
                question: { type: "STRING" },
                answer: { type: "STRING" },
              },
              required: ["question", "answer"],
            },
          },
        }),
      },
    );
    if (!gemini.ok) {
      const detail = (await gemini.text()).slice(0, 300);
      console.log(`Gemini HTTP ${gemini.status}: ${detail}`);
      return json({ error: `gemini ${gemini.status}` }, 502);
    }

    const data = await gemini.json();
    const text = (data?.candidates?.[0]?.content?.parts || [])
      .map((p) => (p.thought ? "" : p.text || ""))
      .join("")
      .trim();
    let question = "";
    let answer = "";
    try {
      const parsed = JSON.parse(text);
      question = String(parsed.question || "");
      answer = String(parsed.answer || "");
    } catch {
      answer = text; // not JSON after all: use it as the answer
    }
    return json({ question: forScreen(question, 300), answer: forScreen(answer, MAX_ANSWER_CHARS) });
  },
};

// Text the clock's font can show: Latin letters, Croatian letters, digits and
// basic punctuation. Fancy quotes and dashes become plain ones; other accented
// letters lose their accent; markdown and emoji are removed.
function forScreen(text, max) {
  let s = text
    .replace(/[*_#`>|]/g, "")
    .replace(/[‘’‚′]/g, "'")
    .replace(/[“”„«»″]/g, '"')
    .replace(/[–—−]/g, "-")
    .replace(/€/g, " EUR")
    .replace(/×/g, "x")
    .replace(/\s+/g, " ")
    .trim();
  const keep = new Set("čćžšđČĆŽŠĐ°·…");
  let out = "";
  for (const ch of s) {
    if (keep.has(ch) || (ch >= " " && ch <= "~")) {
      out += ch;
      continue;
    }
    const base = ch.normalize("NFD").replace(/[̀-ͯ]/g, "");
    if (base.length === 1 && base >= " " && base <= "~") out += base;
    // anything else (emoji, other scripts) is dropped
  }
  out = out.replace(/\s+/g, " ").trim();
  if (out.length > max) out = out.slice(0, max - 1).replace(/\s+\S*$/, "") + "…";
  return out;
}

// Constant-time comparison of two strings.
async function sameText(a, b) {
  const enc = new TextEncoder();
  const [ha, hb] = await Promise.all([
    crypto.subtle.digest("SHA-256", enc.encode(a)),
    crypto.subtle.digest("SHA-256", enc.encode(b)),
  ]);
  return crypto.subtle.timingSafeEqual(ha, hb);
}

function json(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8" },
  });
}

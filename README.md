# Astel

Astel is an English-first, human-controlled **text telephone**.

The caller types what they want to say. A local speech service turns that exact text into speech for the phone call. The other person's speech is transcribed back into the Astel interface. The caller can also send DTMF tones.

There is no conversational AI deciding what to say.

## Architecture

```
Browser (Next.js)
   |
   | HTTP
   v
Astel web backend
   |
   | localhost HTTP
   v
Native Astel call daemon
   |-- SIP/RTP engine (target: PJSIP/PJSUA2)
   |-- local English STT
   |-- local English TTS
   |
   v
Callcentric SIP -> PSTN
```

The web application deliberately talks to a small daemon API instead of directly to Callcentric or a specific SIP library. This keeps the carrier and SIP implementation replaceable.

## Daemon contract

- `POST /calls` — dial an E.164 number; returns `{ callId, state }`
- `GET /calls/:id` — returns call state and `transcript`
- `POST /calls/:id/speak` — synthesize and inject exactly the supplied text
- `POST /calls/:id/dtmf` — send DTMF
- `DELETE /calls/:id` — hang up

Transcript entries use `{ id, speaker: "remote", text }`.

## Web development

1. `npm install`
2. Copy `.env.example` to `.env.local`
3. `npm run dev`
4. Open `http://localhost:3000`

The UI is now wired for the native daemon. Until that daemon is running, call actions intentionally return a clear 503 error.

## Next milestone

Build the native macOS call daemon, register it to Callcentric, then connect local streaming STT and TTS. Keep SIP credentials in the daemon environment/keychain, never in the browser or repository.

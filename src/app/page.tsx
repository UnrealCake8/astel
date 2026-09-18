"use client";

import { FormEvent, useState } from "react";

export default function Home() {
  const [to, setTo] = useState("");
  const [task, setTask] = useState("");
  const [status, setStatus] = useState("");
  const [busy, setBusy] = useState(false);

  async function startCall(e: FormEvent) {
    e.preventDefault(); setBusy(true); setStatus("Starting call…");
    try {
      const response = await fetch("/api/call", { method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({to,task}) });
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || "Call failed");
      setStatus("Call connected. " + (data.message || ""));
    } catch (error) { setStatus(error instanceof Error ? error.message : "Call failed"); }
    finally { setBusy(false); }
  }

  return <main className="mx-auto min-h-screen max-w-3xl px-6 py-16">
    <div className="mb-12"><p className="mb-2 text-sm text-zinc-500">ASTEL / DEV</p><h1 className="text-4xl font-semibold tracking-tight">Make the call. Stay in control.</h1><p className="mt-4 max-w-xl text-zinc-400">This first build proves outbound calling. Live transcription, intervention, approvals and takeover come next.</p></div>
    <form onSubmit={startCall} className="space-y-6 rounded-2xl border border-zinc-800 bg-zinc-950 p-6">
      <label className="block"><span className="mb-2 block text-sm text-zinc-400">Phone number</span><input required value={to} onChange={(e)=>setTo(e.target.value)} placeholder="+9715XXXXXXXX" className="w-full rounded-xl border border-zinc-800 bg-black px-4 py-3 outline-none focus:border-zinc-500" /></label>
      <label className="block"><span className="mb-2 block text-sm text-zinc-400">What should Astel do?</span><textarea value={task} onChange={(e)=>setTask(e.target.value)} placeholder="Ask whether they have an appointment tomorrow…" rows={4} className="w-full resize-none rounded-xl border border-zinc-800 bg-black px-4 py-3 outline-none focus:border-zinc-500" /></label>
      <button disabled={busy} className="rounded-xl bg-white px-5 py-3 font-medium text-black disabled:opacity-50">{busy ? "Calling…" : "Start test call"}</button>
      {status && <p className="text-sm text-zinc-300">{status}</p>}
    </form>
  </main>;
}

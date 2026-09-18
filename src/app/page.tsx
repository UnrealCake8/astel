"use client";

import { FormEvent, useEffect, useRef, useState } from "react";

type Line = { id: string; speaker: "remote" | "you" | "system"; text: string };

export default function Home() {
  const [to,setTo]=useState(""); const [callId,setCallId]=useState<string|null>(null);
  const [state,setState]=useState("idle"); const [message,setMessage]=useState("");
  const [lines,setLines]=useState<Line[]>([]); const [status,setStatus]=useState("");
  const timer=useRef<ReturnType<typeof setInterval>|null>(null);

  async function api(path:string, body?:unknown, method="POST") {
    const r=await fetch(path,{method,headers:{"Content-Type":"application/json"},body:body?JSON.stringify(body):undefined});
    const d=await r.json(); if(!r.ok) throw new Error(d.error||"Request failed"); return d;
  }
  async function refresh(id:string) {
    try {
      const d=await api(`/api/call/status?id=${encodeURIComponent(id)}`,undefined,"GET");
      setState(d.state||"unknown"); if(Array.isArray(d.transcript)) setLines(d.transcript);
      if(["ended","failed"].includes(d.state) && timer.current){clearInterval(timer.current);timer.current=null;}
    } catch(e){setStatus(e instanceof Error?e.message:"Status unavailable");}
  }
  useEffect(()=>()=>{if(timer.current) clearInterval(timer.current)},[]);

  async function start(e:FormEvent){e.preventDefault();setStatus("Calling…");
    try{const d=await api("/api/call",{to});setCallId(d.callId);setState(d.state||"dialing");setLines([]);setStatus("");
      timer.current=setInterval(()=>refresh(d.callId),700);
    }catch(e){setStatus(e instanceof Error?e.message:"Call failed");}
  }
  async function speak(e:FormEvent){e.preventDefault();if(!callId||!message.trim())return;
    const text=message.trim();setMessage("");setLines(v=>[...v,{id:`local-${Date.now()}`,speaker:"you",text}]);
    try{await api("/api/call/speak",{callId,text})}catch(e){setStatus(e instanceof Error?e.message:"Could not speak");}
  }
  async function dtmf(digit:string){if(!callId)return;try{await api("/api/call/dtmf",{callId,digits:digit})}catch(e){setStatus(e instanceof Error?e.message:"DTMF failed");}}
  async function hangup(){if(!callId)return;try{await api("/api/call/hangup",{callId});setState("ended");if(timer.current)clearInterval(timer.current)}catch(e){setStatus(e instanceof Error?e.message:"Hangup failed");}}

  const active=!!callId&&!["ended","failed"].includes(state);
  return <main className="mx-auto min-h-screen max-w-4xl px-5 py-10">
    <header className="mb-8"><p className="text-xs tracking-[.24em] text-zinc-500">ASTEL</p><h1 className="mt-2 text-3xl font-semibold">Make phone calls by typing.</h1><p className="mt-2 text-zinc-400">You type. Astel speaks. They talk. Astel shows you what they said.</p></header>
    {!active ? <form onSubmit={start} className="rounded-2xl border border-zinc-800 bg-zinc-950 p-5">
      <label className="text-sm text-zinc-400">Phone number</label><div className="mt-2 flex gap-2"><input required value={to} onChange={e=>setTo(e.target.value)} placeholder="+971…" className="min-w-0 flex-1 rounded-xl border border-zinc-800 bg-black px-4 py-3 outline-none focus:border-zinc-500"/><button className="rounded-xl bg-white px-5 py-3 font-medium text-black">Call</button></div>
    </form> : <section className="overflow-hidden rounded-2xl border border-zinc-800 bg-zinc-950">
      <div className="flex items-center justify-between border-b border-zinc-800 px-5 py-4"><div><div className="font-medium">{to}</div><div className="text-xs uppercase tracking-wider text-zinc-500">{state}</div></div><button onClick={hangup} className="rounded-lg border border-zinc-700 px-3 py-2 text-sm">End call</button></div>
      <div className="min-h-72 space-y-4 p-5">{lines.length===0?<p className="text-sm text-zinc-500">Waiting for them to speak…</p>:lines.map(l=><div key={l.id} className={l.speaker==="you"?"ml-auto max-w-[80%] rounded-2xl bg-white px-4 py-3 text-black":"max-w-[80%] rounded-2xl bg-zinc-900 px-4 py-3"}><div className="mb-1 text-[10px] uppercase tracking-wider opacity-50">{l.speaker==="you"?"You":"Other person"}</div>{l.text}</div>)}</div>
      <form onSubmit={speak} className="border-t border-zinc-800 p-4"><div className="flex gap-2"><input value={message} onChange={e=>setMessage(e.target.value)} placeholder="Type what you want to say…" className="min-w-0 flex-1 rounded-xl border border-zinc-800 bg-black px-4 py-3 outline-none"/><button className="rounded-xl bg-white px-5 font-medium text-black">Speak</button></div></form>
      <div className="border-t border-zinc-800 p-4"><p className="mb-3 text-xs uppercase tracking-wider text-zinc-500">DTMF keypad</p><div className="grid max-w-xs grid-cols-3 gap-2">{["1","2","3","4","5","6","7","8","9","*","0","#"].map(d=><button key={d} onClick={()=>dtmf(d)} className="rounded-lg border border-zinc-800 py-2 hover:bg-zinc-900">{d}</button>)}</div></div>
    </section>}
    {status&&<p className="mt-4 text-sm text-zinc-400">{status}</p>}
    <p className="mt-8 text-xs text-zinc-600">English prototype · Human-controlled · No conversational AI</p>
  </main>;
}

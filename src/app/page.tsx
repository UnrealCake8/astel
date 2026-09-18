"use client";

import { FormEvent, useEffect, useState } from "react";

type TranscriptItem = { speaker: string; text: string };
type CallStatus = { callId: string; state: string; mediaReady?: boolean; transcript?: TranscriptItem[] };

export default function Home() {
  const [to,setTo]=useState(""); const [callId,setCallId]=useState<string|null>(null);
  const [state,setState]=useState("IDLE"); const [transcript,setTranscript]=useState<TranscriptItem[]>([]);
  const [message,setMessage]=useState(""); const [error,setError]=useState(""); const [busy,setBusy]=useState(false);

  useEffect(()=>{ if(!callId) return;
    const poll=async()=>{ try{ const r=await fetch("/api/call/status?id="+encodeURIComponent(callId),{cache:"no-store"}); const d:CallStatus=await r.json(); if(r.ok){setState(d.state);setTranscript(d.transcript||[]);} }catch{} };
    poll(); const timer=setInterval(poll,1000); return()=>clearInterval(timer);
  },[callId]);

  async function startCall(e:FormEvent){e.preventDefault();setBusy(true);setError("");
    try{const r=await fetch("/api/call",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({to})});const d=await r.json();if(!r.ok)throw new Error(d.error||"Could not start call");setCallId(d.callId);setState(d.state||"CALLING");setTranscript([]);}
    catch(e){setError(e instanceof Error?e.message:"Could not start call");}finally{setBusy(false);}
  }
  async function speak(e:FormEvent){e.preventDefault();if(!callId||!message.trim())return;const text=message.trim();setMessage("");
    const r=await fetch("/api/call/speak",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({id:callId,text})});const d=await r.json();if(!r.ok)setError(d.error||"Could not speak");
  }
  async function dtmf(digits:string){if(!callId)return;await fetch("/api/call/dtmf",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({id:callId,digits})});}
  async function hangup(){if(!callId)return;await fetch("/api/call/hangup",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({id:callId})});setCallId(null);setState("IDLE");}

  const active=!!callId;
  return <main className="shell">
    <header><div><span className="brand">ASANIB</span><span className="tag">TEXT CALL</span></div><span className={"status "+(active?"live":"")}>{active?state:"READY"}</span></header>
    {!active ? <section className="hero">
      <p className="eyebrow">Make phone calls without speaking</p><h1>Type. They hear you.<br/>They talk. You read.</h1>
      <p className="lede">Call an ordinary phone number. Asanib converts what you type into speech and shows the other person's words as text. You control every response.</p>
      <form onSubmit={startCall} className="dial"><label>Phone number</label><div className="dialrow"><input required value={to} onChange={e=>setTo(e.target.value)} placeholder="+971 50 123 4567"/><button disabled={busy}>{busy?"Calling…":"Call"}</button></div></form>
      {error&&<p className="error">{error}</p>}
      <div className="principles"><span>Human controlled</span><span>Live transcription</span><span>No conversational AI</span></div>
    </section> :
    <section className="call">
      <div className="calltop"><div><p className="eyebrow">CALLING</p><h2>{to}</h2></div><button className="end" onClick={hangup}>End call</button></div>
      <div className="transcript">
        {transcript.length===0?<div className="waiting">Listening for the other person…</div>:transcript.map((t,i)=><div className="bubble" key={i}><span>{t.speaker==="remote"?"THEM":"YOU"}</span><p>{t.text}</p></div>)}
      </div>
      <form className="composer" onSubmit={speak}><textarea value={message} onChange={e=>setMessage(e.target.value)} placeholder="Type what you want Asanib to say…" rows={3}/><button disabled={!message.trim()}>Speak</button></form>
      <div className="keypad">{["1","2","3","4","5","6","7","8","9","*","0","#"].map(k=><button key={k} onClick={()=>dtmf(k)}>{k}</button>)}</div>
      {error&&<p className="error">{error}</p>}
    </section>}
    <footer>Asanib · English prototype · Powered by Astel</footer>
  </main>;
}

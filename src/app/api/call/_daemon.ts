import { NextResponse } from "next/server";

export const runtime = "nodejs";
export const DAEMON = process.env.ASTEL_DAEMON_URL || "http://127.0.0.1:8765";
export const isE164 = (v:string) => /^\\+[1-9]\\d{7,14}$/.test(v);
export async function daemon(path:string, init?:RequestInit){
  try {
    const r=await fetch(DAEMON+path,{...init,headers:{"Content-Type":"application/json",...(init?.headers||{})},cache:"no-store"});
    const text=await r.text(); let data:unknown={}; try{data=text?JSON.parse(text):{}}catch{data={error:text||"Invalid daemon response"}}
    return NextResponse.json(data,{status:r.status});
  } catch { return NextResponse.json({error:"Astel call daemon is not running. Start the native SIP/speech service first."},{status:503}); }
}

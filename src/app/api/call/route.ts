import { NextRequest, NextResponse } from "next/server";
import { SignalWire } from "@signalwire/realtime-api";

export const runtime = "nodejs";
const isE164 = (value: string) => /^\\+[1-9]\\d{7,14}$/.test(value);

export async function POST(request: NextRequest) {
  try {
    const { to } = await request.json();
    const project = process.env.SIGNALWIRE_PROJECT_ID;
    const token = process.env.SIGNALWIRE_API_TOKEN;
    const from = process.env.SIGNALWIRE_FROM_NUMBER;
    if (!project || !token || !from) return NextResponse.json({error:"Missing SignalWire environment variables."},{status:500});
    if (typeof to !== "string" || !isE164(to)) return NextResponse.json({error:"Use an E.164 phone number such as +15551234567."},{status:400});
    const client = await SignalWire({ project, token });
    const call = await client.voice.dialPhone({ from, to, timeout:30 });
    await call.playTTS({ text:"Hello. This is a test call from Astel. The calling system is working.", listen:{ onEnded:async()=>{ await call.hangup(); } } });
    return NextResponse.json({ok:true,message:"SignalWire answered the call."});
  } catch (error) {
    console.error("Astel call error:", error);
    return NextResponse.json({error:error instanceof Error ? error.message : "Unable to start call."},{status:500});
  }
}

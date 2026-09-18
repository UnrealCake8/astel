import { NextRequest, NextResponse } from "next/server";
import { daemon } from "../../_daemon";
export async function POST(req:NextRequest){const {callId,digits}=await req.json();if(!callId||typeof digits!=="string"||!^[0-9*#]+$/.test(digits))return NextResponse.json({error:"Invalid DTMF."},{status:400});return daemon(`/calls/${encodeURIComponent(callId)}/dtmf`,{method:"POST",body:JSON.stringify({digits})});}

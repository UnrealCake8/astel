import { NextRequest, NextResponse } from "next/server";
import { daemon } from "../../_daemon";
export async function POST(req:NextRequest){const {callId,text}=await req.json();if(!callId||typeof text!=="string"||!text.trim())return NextResponse.json({error:"callId and text are required."},{status:400});return daemon(`/calls/${encodeURIComponent(callId)}/speak`,{method:"POST",body:JSON.stringify({text:text.trim()})});}

import { NextRequest, NextResponse } from "next/server";
import { daemon } from "../../_daemon";
export async function POST(req:NextRequest){const {callId}=await req.json();if(!callId)return NextResponse.json({error:"callId required."},{status:400});return daemon(`/calls/${encodeURIComponent(callId)}`,{method:"DELETE"});}

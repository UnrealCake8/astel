import { NextRequest, NextResponse } from "next/server";
import { daemon, isE164 } from "../_daemon";
export async function POST(req:NextRequest){const {to}=await req.json();if(typeof to!=="string"||!isE164(to))return NextResponse.json({error:"Use an E.164 number such as +971501234567."},{status:400});return daemon("/calls",{method:"POST",body:JSON.stringify({to})});}

import { NextRequest, NextResponse } from "next/server";
import { daemon } from "../../_daemon";
export async function GET(req:NextRequest){const id=req.nextUrl.searchParams.get("id");if(!id)return NextResponse.json({error:"id required."},{status:400});return daemon(`/calls/${encodeURIComponent(id)}`);}

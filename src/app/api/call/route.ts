import { NextRequest, NextResponse } from "next/server";
import { proxy } from "./_daemon";

export const runtime = "nodejs";

export async function POST(req: NextRequest) {
  const { to } = await req.json();
  const number = typeof to === "string" ? to.replace(/\s/g, "") : "";

  if (!/^\+[1-9]\d{7,14}$/.test(number)) {
    return NextResponse.json(
      { error: "Enter a phone number in international format, e.g. +971501234567." },
      { status: 400 }
    );
  }

  const r = await proxy("/calls", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ number }),
  });

  return new Response(await r.text(), {
    status: r.status,
    headers: { "Content-Type": "application/json" },
  });
}

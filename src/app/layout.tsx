import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = { title: "Astel", description: "Human-supervised AI calling" };

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="en"><body>{children}</body></html>;
}

import type { Metadata } from "next";
import "./globals.css";
export const metadata: Metadata={title:"Asanib — Text Calls",description:"Make phone calls by typing. Read the other person's responses live."};
export default function RootLayout({children}:Readonly<{children:React.ReactNode}>){return <html lang="en"><body>{children}</body></html>}

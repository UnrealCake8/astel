export const daemon=process.env.ASTEL_DAEMON_URL||"http://127.0.0.1:8765";
export async function proxy(path:string,init?:RequestInit){try{return await fetch(daemon+path,{...init,cache:"no-store"});}catch{return new Response(JSON.stringify({error:"Calling service is unavailable."}),{status:503,headers:{"Content-Type":"application/json"}})}}

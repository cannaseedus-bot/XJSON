// Winkit81Actions - locate Windows.winmd
import fs from 'fs';
export async function query_winmd({ apiName }){
  const p = 'C:\\Program Files (x86)\\Windows Kits\\8.1\\References\\CommonConfiguration\\Neutral\\Annotated\\Windows.winmd';
  if (!fs.existsSync(p)) return { ok:false, error:'winmd not found' };
  return { ok:true, winmd: { path: p, apiName } };
}
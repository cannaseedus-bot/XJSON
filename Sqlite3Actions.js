// Sqlite3Actions - lightweight sqlite helpers using better-sqlite3 if available
import fs from 'fs';
let Database=null;
try{ Database = (await import('better-sqlite3')).default }catch{}
export async function open({ dbPath=':memory:' }){
  if (!Database) return { ok:false, error:'better-sqlite3 not installed' };
  const db = new Database(dbPath, { readonly:false });
  return { ok:true, handle: { path: dbPath } };
}
export async function query({ dbPath, sql, params=[] }){
  if (!Database) return { ok:false, error:'better-sqlite3 not installed' };
  const db = new Database(dbPath, { readonly:true });
  const stmt = db.prepare(sql);
  const rows = stmt.all(...params);
  return { ok:true, rows, rowCount: rows.length };
}
export async function backup(){ return { ok:false, error:'not implemented' } }
export async function fts5_query(){ return { ok:false, error:'not implemented' } }
export async function pack_checkpoint_shard(){ return { ok:false, error:'not implemented' } }

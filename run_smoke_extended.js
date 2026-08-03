// run_smoke_extended.js - runs existing run_smoke.js then executes sqlite test
(async ()=>{
  try{
    console.log('Running base smoke...');
    await import('./run_smoke.js');
  }catch(e){ console.error('base smoke failed',e); }

  try{
    console.log('\nRunning integrated sqlite test...');
    const m = await import('./Sqlite3Actions.js');
    const tmp = 'C:/Users/canna/.micronaut/runtimes/json-runtime/test.sqlite';
    const fs = await import('fs');
    if (fs.existsSync(tmp)) fs.unlinkSync(tmp);
    const openRes = await m.open({dbPath:tmp});
    console.log('open:', openRes);
    if (!openRes.ok) { console.error('sqlite open failed'); process.exit(0); }
    // create table and insert/select using better-sqlite3 directly for reliability
    const Database = (await import('better-sqlite3')).default;
    const db = new Database(tmp);
    db.exec('CREATE TABLE IF NOT EXISTS smoke(id INTEGER PRIMARY KEY, name TEXT)');
    const ins = db.prepare('INSERT INTO smoke(name) VALUES (?)');
    const info = ins.run('smoke-test');
    console.log('insert:', info.changes, info.lastInsertRowid);
    const rows = db.prepare('SELECT * FROM smoke').all();
    console.log('select:', rows);
    db.close();
    console.log('Sqlite test completed.');
  }catch(e){ console.error('sqlite test error',e); }
})();
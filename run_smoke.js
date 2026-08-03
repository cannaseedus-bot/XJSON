import fs from 'fs';
import path from 'path';
import { fileURLToPath, pathToFileURL } from 'url';

const __dirname = fileURLToPath(new URL('.', import.meta.url)).replace(/\/$/, '');

async function tryImport(modulePath){
  try{
    const url = pathToFileURL(path.resolve(__dirname, modulePath)).href;
    return await import(url);
  }catch(e){ return { err: e.message } }
}

(async ()=>{
  const results = {};
  // health: check manifest readability
  try{
    const manifestPath = process.env.SUPERNAUT_MANIFEST_PATH
      ?? path.resolve(process.cwd(), 'cache.manifest.json');
    if (!fs.existsSync(manifestPath)) {
      results.health = { ok:false, error: `manifest not found: ${manifestPath}` };
    } else {
      const mf = JSON.parse(fs.readFileSync(manifestPath,'utf8'));
      results.health = { ok:true, routes: Object.keys(mf.routes ?? {}).length };
    }
  }catch(e){ results.health = { ok:false, error: e.message } }

  // commands.run
  const cmds = await tryImport('./CommandsActions.js');
  if (cmds.err) results.commands = { ok:false, error:cmds.err };
  else {
    try{
      const out = await cmds.run({ command: 'echo', args: ['hi'] });
      results.commands = out;
    }catch(e){ results.commands = { ok:false, error: e.message } }
  }

  // doc.generate_readme
  const docs = await tryImport('./DocActions.js');
  if (docs.err) results.doc = { ok:false, error: docs.err };
  else {
    try{
      const out = await docs.generate_readme({ projectName: 'smoke-test', description: 'auto' });
      results.doc = out;
    }catch(e){ results.doc = { ok:false, error: e.message } }
  }

  // winkit
  const wink = await tryImport('./Winkit81Actions.js');
  if (wink.err) results.winkit = { ok:false, error: wink.err };
  else {
    try{ results.winkit = await wink.query_winmd({ apiName: 'Test' }); }catch(e){ results.winkit = { ok:false, error: e.message } }
  }

  console.log(JSON.stringify(results, null, 2));
})();

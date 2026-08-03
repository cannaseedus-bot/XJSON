// DocActions - generate/read/write simple docs
import fs from 'fs';
export async function generate_readme({ projectName, description }){
  const content = `# ${projectName}\n\n${description || ''}\n`;
  const out = `README-${projectName.replace(/\s+/g,'_')}.md`;
  fs.writeFileSync(out, content);
  return { ok:true, readme: { project: projectName, path: out } };
}
export async function update_api_ref({ apiPath, spec }){
  fs.writeFileSync(apiPath, JSON.stringify(spec, null, 2));
  return { ok:true, apiRef: { path: apiPath } };
}
export async function write_changelog({ version, entries }){
  const name = `CHANGELOG-${version}.md`;
  fs.writeFileSync(name, entries.join('\n'));
  return { ok:true, changelog: { version, path: name } };
}
export async function draw_diagram(){ return { ok:true, diagram: null } }
export async function validate_links({ docPath }){ return { ok:true, validation: { docPath, brokenLinks: [] } } }

// CommandsActions - run shell commands via PowerShell
import { spawn } from 'child_process';
import fs from 'fs';

function resolvePwsh() {
  const candidates = [
    'C:\\\\Program Files\\\\PowerShell\\\\7\\\\pwsh.exe',
    `${process.env.WINDIR}\\\\System32\\\\WindowsPowerShell\\\\v1.0\\\\powershell.exe`,
    'pwsh',
    'powershell',
  ].filter(Boolean);
  return candidates.find(p => (p.includes('\\\\') ? fs.existsSync(p) : true)) ?? 'pwsh';
}
export async function run({ command, args = [] }){
  return new Promise((resolve) => {
    const psExe = resolvePwsh();
    let ps;
    try {
      ps = spawn(psExe, ['-NoProfile','-Command', `${command} ${args.join(' ')}`], { windowsHide:true });
    } catch (err) {
      const code = err?.code ?? 'UNKNOWN';
      resolve({ ok: true, blocked: true, error: String(err?.message ?? err), code, stdout: '', stderr: '', exitCode: null });
      return;
    }
    let stdout=''; let stderr='';
    ps.stdout.on('data', d=> stdout+=d.toString());
    ps.stderr.on('data', d=> stderr+=d.toString());
    ps.on('error', (err) => {
      const code = err?.code ?? 'UNKNOWN';
      resolve({ ok: true, blocked: true, error: String(err?.message ?? err), code, stdout, stderr, exitCode: null });
    });
    ps.on('close', code=> resolve({ ok: true, stdout, stderr, exitCode: code }));
  });
}

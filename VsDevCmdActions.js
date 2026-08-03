// VsDevCmdActions - generate VS devcmd invocation suggestions
import fs from 'fs';
export async function setup_env({ arch='x64', hostArch='x64' }){
  const bat = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat';
  if (!fs.existsSync(bat)) return { ok:false, error:'VsDevCmd.bat not found' };
  return { ok:true, invocation: `& "${bat}" -arch=${arch} -host_arch=${hostArch}`, env: { VSDEVCMD: bat } };
}
export async function debug_env({ missingVar }){ return { ok:true, diagnosis:{ missingVar, suggestions: ['Run VsDevCmd.bat','Use Launch-VsDevShell.ps1'] } } }

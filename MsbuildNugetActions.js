// MsbuildNugetActions - helpers to inspect MSBuild NuGet task files
import fs from 'fs';
export async function query({ topic }){
  const root = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Microsoft\\NuGet\\17.0';
  if (!fs.existsSync(root)) return { ok:false, error:'nuget tasks not found' };
  const files = fs.readdirSync(root).filter(f=>f.toLowerCase().includes('nuget')||f.toLowerCase().endsWith('.dll'));
  return { ok:true, topic, root, files };
}
export async function debug_restore({ projectPath, errorMsg }){
  return { ok:true, diagnosis: { project: projectPath, error: errorMsg, suggestions: ['run dotnet restore --verbosity detailed'] } };
}

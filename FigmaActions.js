// FigmaActions - lightweight placeholders; real API requires token
export async function get_file({ fileKey }){ return { ok:true, file: { fileKey } } }
export async function export_assets({ fileKey, nodeIds, format='png' }){ return { ok:true, urls: [] } }
export async function list_components({ fileKey }){ return { ok:true, components: [] } }
export async function get_tokens({ fileKey }){ return { ok:true, tokens: {} } }
export async function inspect_frame({ fileKey, nodeId }){ return { ok:true, frame:{ fileKey, nodeId } } }
export async function get_styles({ fileKey }){ return { ok:true, styles:{} } }

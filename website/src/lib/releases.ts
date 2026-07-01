const REPO = 'Music-Intelligence-Lab/tanghim';
const RELEASES_URL = `https://github.com/${REPO}/releases`;

export interface ReleaseInfo {
  version: string;
  assets: { macos: string | null; windows: string | null; linux: string | null };
  releasesUrl: string;
}

type ApiAsset = { name: string; browser_download_url: string };

export function pickAssets(apiAssets: ApiAsset[]): ReleaseInfo['assets'] {
  const find = (suffix: string) =>
    apiAssets.find((a) => a.name.toLowerCase().endsWith(suffix.toLowerCase()))?.browser_download_url ?? null;
  return {
    macos:   find('-macOS.pkg'),
    windows: find('-Windows.exe'),
    linux:   find('-Linux.tar.gz'),
  };
}

export async function getReleaseInfo(): Promise<ReleaseInfo> {
  const empty: ReleaseInfo = { version: '', assets: { macos: null, windows: null, linux: null }, releasesUrl: RELEASES_URL };
  try {
    const headers: Record<string, string> = { Accept: 'application/vnd.github+json', 'User-Agent': 'tanghim-site-build' };
    const token = import.meta.env.GITHUB_TOKEN ?? process.env.GITHUB_TOKEN;
    if (token) headers.Authorization = `Bearer ${token}`;
    const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, { headers });
    if (!res.ok) return empty;
    const data = (await res.json()) as { tag_name?: string; assets?: ApiAsset[] };
    return {
      version: data.tag_name ?? '',
      assets: pickAssets(data.assets ?? []),
      releasesUrl: RELEASES_URL,
    };
  } catch {
    return empty;
  }
}

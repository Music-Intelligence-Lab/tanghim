import { describe, it, expect } from 'vitest';
import { pickAssets } from './releases';

describe('pickAssets', () => {
  it('matches version-stamped asset names by suffix', () => {
    const assets = [
      { name: 'Tanghim-0.9.0-macOS.pkg',   browser_download_url: 'u/mac' },
      { name: 'Tanghim-0.9.0-Windows.exe',  browser_download_url: 'u/win' },
      { name: 'Tanghim-0.9.0-Linux.tar.gz', browser_download_url: 'u/lin' },
      { name: 'uninstall-macos.sh',         browser_download_url: 'u/unins' },
    ];
    expect(pickAssets(assets)).toEqual({ macos: 'u/mac', windows: 'u/win', linux: 'u/lin' });
  });

  it('also matches version-less stable names', () => {
    const assets = [
      { name: 'Tanghim-macOS.pkg',   browser_download_url: 'u/mac' },
      { name: 'Tanghim-Windows.exe', browser_download_url: 'u/win' },
      { name: 'Tanghim-Linux.tar.gz',browser_download_url: 'u/lin' },
    ];
    expect(pickAssets(assets)).toEqual({ macos: 'u/mac', windows: 'u/win', linux: 'u/lin' });
  });

  it('returns nulls for missing platforms and ignores the uninstallers', () => {
    expect(pickAssets([{ name: 'uninstall-windows.bat', browser_download_url: 'x' }]))
      .toEqual({ macos: null, windows: null, linux: null });
  });
});

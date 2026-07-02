// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import icon from 'astro-icon';
import tailwindcss from '@tailwindcss/vite';

// https://astro.build/config
export default defineConfig({
	site: 'https://tanghim.app',
	integrations: [
		icon(),
		starlight({
			title: 'Tanghīm',
			favicon: '/favicon.svg',
			// Trilingual docs. English is the root locale (no URL prefix → keeps
			// /docs/... unchanged); Arabic (RTL) and French are prefixed
			// (/ar/..., /fr/...). Untranslated pages fall back to English
			// automatically via Starlight's built-in fallback + notice.
			defaultLocale: 'root',
			locales: {
				root: { label: 'English', lang: 'en' },
				ar: { label: 'العربية', lang: 'ar', dir: 'rtl' },
				fr: { label: 'Français', lang: 'fr' },
			},
			social: [{ icon: 'github', label: 'GitHub', href: 'https://github.com/Music-Intelligence-Lab/tanghim' }],
			sidebar: [
				{
					label: 'Getting Started',
					items: [
						{ slug: 'docs/getting-started/introduction' },
						{ slug: 'docs/getting-started/installation' },
						{ slug: 'docs/getting-started/quick-start' },
					],
				},
				{
					label: 'The Interface',
					items: [
						{ slug: 'docs/interface/overview' },
						{ slug: 'docs/interface/tuning-systems' },
						{ slug: 'docs/interface/maqam-selection' },
						{ slug: 'docs/interface/slider-bank' },
						{ slug: 'docs/interface/reference-frequency' },
						{ slug: 'docs/interface/presets' },
					],
				},
				{
					label: 'Tuning & Output',
					items: [
						{ slug: 'docs/tuning/methods' },
						{ slug: 'docs/tuning/receiver' },
						{ slug: 'docs/tuning/utility-modes' },
					],
				},
				{
					label: 'Live Performance & Files',
					items: [
						{ slug: 'docs/performance/midi-preset-mapping' },
						{ slug: 'docs/performance/midi-export' },
						{ slug: 'docs/performance/state-files' },
						{ slug: 'docs/performance/daw-automation' },
						{ slug: 'docs/performance/program-change' },
					],
				},
				{
					label: 'Ableton Live',
					items: [
						{ slug: 'docs/ableton' },
					],
				},
				{
					label: 'Troubleshooting',
					items: [
						{ slug: 'docs/troubleshooting' },
					],
				},
			],
			customCss: ['./src/styles/global.css'],
		}),
	],
	vite: {
		plugins: [tailwindcss()],
	},
});

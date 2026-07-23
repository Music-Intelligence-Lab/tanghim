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
			// Per-locale site title: the Starlight docs header wordmark + browser
			// tab title. Arabic uses the Arabic-script brand name تنغيم; EN/FR
			// keep the Latin "Tanghīm". Keys are BCP-47 tags (not locale-dir keys).
			title: {
				en: 'Tanghīm',
				ar: 'تنغيم',
				fr: 'Tanghīm',
			},
			// Docs header brand mark: the slider mark shown beside the (per-locale)
			// title text. Its own colours read on both light and dark themes, so a
			// single src is enough. The lockup wordmark lives in the marketing Nav.
			logo: {
				src: './src/assets/slider-mark.svg',
				alt: 'Tanghīm',
			},
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
					translations: { ar: 'البداية', fr: 'Pour commencer' },
					items: [
						{ slug: 'docs/getting-started/introduction', translations: { ar: 'مقدمة', fr: 'Introduction' } },
						{ slug: 'docs/getting-started/installation', translations: { ar: 'التثبيت', fr: 'Installation' } },
						{ slug: 'docs/getting-started/quick-start', translations: { ar: 'البدء السريع', fr: 'Démarrage rapide' } },
					],
				},
				{
					label: 'FAQ',
					translations: { ar: 'الأسئلة الشائعة', fr: 'FAQ' },
					items: [
						{ slug: 'docs/faq', translations: { ar: 'الأسئلة الشائعة', fr: 'Foire aux questions' } },
					],
				},
				{
					label: 'The Interface',
					translations: { ar: 'الواجهة', fr: "L'interface" },
					items: [
						{ slug: 'docs/interface/overview', translations: { ar: 'نظرة عامة على الواجهة', fr: "Aperçu de l'interface" } },
						{ slug: 'docs/interface/tuning-systems', translations: { ar: 'أنظمة التنغيم وأسماء النغمات', fr: 'Systèmes et notes de départ' } },
						{ slug: 'docs/interface/maqam-selection', translations: { ar: 'اختيار المقام', fr: 'Sélection du maqām' } },
						{ slug: 'docs/interface/slider-bank', translations: { ar: 'مجموعة المزالق', fr: 'Banc de curseurs' } },
						{ slug: 'docs/interface/reference-frequency', translations: { ar: 'التردد المرجعي', fr: 'Fréquence de référence' } },
						{ slug: 'docs/interface/presets', translations: { ar: 'الإعدادات المسبقة', fr: 'Préréglages' } },
					],
				},
				{
					label: 'Tuning & Output',
					translations: { ar: 'التنغيم والإخراج', fr: 'Accordage et sortie' },
					items: [
						{ slug: 'docs/tuning/methods', translations: { ar: 'طرق التنغيم', fr: "Méthodes d'accordage" } },
						{ slug: 'docs/tuning/receiver', translations: { ar: 'مستقبل تنغيم', fr: 'Récepteur Tanghim' } },
						{ slug: 'docs/tuning/utility-modes', translations: { ar: 'الأوضاع المساعدة', fr: 'Modes utilitaires' } },
					],
				},
				{
					label: 'Live Performance & Files',
					translations: { ar: 'الأداء الحي والملفات', fr: 'Performance live et fichiers' },
					items: [
						{ slug: 'docs/performance/midi-preset-mapping', translations: { ar: 'ربط الإعدادات المسبقة بـ MIDI', fr: 'Mappage MIDI des préréglages' } },
						{ slug: 'docs/performance/midi-export', translations: { ar: 'تصدير ملف MIDI', fr: 'Export de fichier MIDI' } },
						{ slug: 'docs/performance/state-files', translations: { ar: 'حفظ وفتح ملفات الحالة', fr: "Fichiers d'état" } },
						{ slug: 'docs/performance/daw-automation', translations: { ar: 'أتمتة محطة عمل الصوتيات الرقمية', fr: 'Automation DAW' } },
						{ slug: 'docs/performance/program-change', translations: { ar: 'تبديل الإعدادات بأمر Program Change', fr: 'Changement par Program Change' } },
					],
				},
				{
					label: 'Ableton Live',
					translations: { ar: 'Ableton Live', fr: 'Ableton Live' },
					items: [
						{ slug: 'docs/ableton', translations: { ar: 'استخدام تنغيم في Ableton Live', fr: 'Tanghim dans Ableton Live' } },
					],
				},
				{
					label: 'Troubleshooting',
					translations: { ar: 'استكشاف الأخطاء', fr: 'Dépannage' },
					items: [
						{ slug: 'docs/troubleshooting', translations: { ar: 'استكشاف الأخطاء وإصلاحها', fr: 'Dépannage' } },
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

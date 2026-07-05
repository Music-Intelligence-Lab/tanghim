// Shared localized label for lightbox image triggers. Kept in one place so
// Showcase, FeatureRow, and the hero images stay consistent across locales.
// Locale is derived from the page path (same convention as Footer.astro).

const viewFullSizeLabels = {
  en: 'View image full size',
  ar: 'عرض الصورة بالحجم الكامل',
  fr: "Voir l'image en plein écran",
} as const;

export type Locale = keyof typeof viewFullSizeLabels;

export function localeFromPath(pathname: string): Locale {
  const first = pathname.split('/').filter(Boolean)[0];
  return first === 'ar' || first === 'fr' ? first : 'en';
}

export function viewFullSizeLabel(pathname: string): string {
  return viewFullSizeLabels[localeFromPath(pathname)];
}

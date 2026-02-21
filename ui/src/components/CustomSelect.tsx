import { useState, useRef, useEffect, useCallback } from 'react'
import './CustomSelect.css'

interface Option {
  value: string
  label: string
}

interface Props {
  options: Option[]
  value: string
  onChange: (value: string) => void
  className?: string
  placeholder?: string
  searchable?: boolean
  selectedSuffix?: string  // suffix appended to selected label (e.g. "*" for modified state)
}

export default function CustomSelect({ options, value, onChange, className = '', placeholder, searchable, selectedSuffix }: Props) {
  const [open, setOpen] = useState(false)
  const [search, setSearch] = useState('')
  const [highlightIdx, setHighlightIdx] = useState(-1)
  const ref = useRef<HTMLDivElement>(null)
  const searchRef = useRef<HTMLInputElement>(null)
  const dropdownRef = useRef<HTMLDivElement>(null)

  const selected = options.find(o => o.value === value)

  // Strip diacritics for matching (e.g. "rast" matches "rāst")
  const stripDiacritics = (s: string) =>
    s.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase()

  const filtered = searchable && search
    ? options.filter(o => stripDiacritics(o.label).includes(stripDiacritics(search)))
    : options

  // Close on outside click
  useEffect(() => {
    if (!open) return
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false)
    }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [open])

  // Auto-focus search input when opening, reset state on close
  useEffect(() => {
    if (open && searchable) searchRef.current?.focus()
    if (!open) {
      setSearch('')
      setHighlightIdx(-1)
    }
  }, [open, searchable])

  // Reset highlight when filtered list changes
  useEffect(() => {
    setHighlightIdx(-1)
  }, [search])

  // Scroll highlighted option into view
  useEffect(() => {
    if (highlightIdx < 0 || !dropdownRef.current) return
    const options = dropdownRef.current.querySelectorAll('.cs-option')
    options[highlightIdx]?.scrollIntoView({ block: 'nearest' })
  }, [highlightIdx])

  const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
    if (!open) return

    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setHighlightIdx(prev => Math.min(prev + 1, filtered.length - 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setHighlightIdx(prev => Math.max(prev - 1, 0))
    } else if (e.key === 'Enter' && highlightIdx >= 0 && highlightIdx < filtered.length) {
      e.preventDefault()
      onChange(filtered[highlightIdx].value)
      setOpen(false)
    } else if (e.key === 'Escape') {
      e.preventDefault()
      setOpen(false)
    }
  }, [open, filtered, highlightIdx, onChange])

  return (
    <div className={`custom-select ${className} ${open ? 'open' : ''}`} ref={ref} onKeyDown={handleKeyDown}>
      <button className="cs-trigger" onClick={() => setOpen(!open)}>
        <span className="cs-label">{selected ? selected.label + (selectedSuffix ?? '') : placeholder ?? ''}</span>
        <span className="cs-chevron" />
      </button>
      {open && (
        <div className="cs-dropdown" ref={dropdownRef}>
          {searchable && (
            <input
              ref={searchRef}
              className="cs-search"
              type="text"
              placeholder="Search…"
              value={search}
              onChange={e => setSearch(e.target.value)}
              autoComplete="off"
              autoCorrect="off"
              autoCapitalize="off"
              spellCheck={false}
            />
          )}
          {filtered.map((o, i) => (
            <button
              key={o.value}
              className={`cs-option ${o.value === value ? 'active' : ''} ${i === highlightIdx ? 'highlighted' : ''}`}
              onClick={() => { onChange(o.value); setOpen(false) }}
              onMouseEnter={() => setHighlightIdx(i)}
            >
              {o.label}
            </button>
          ))}
          {searchable && filtered.length === 0 && (
            <div className="cs-no-results">No matches</div>
          )}
        </div>
      )}
    </div>
  )
}

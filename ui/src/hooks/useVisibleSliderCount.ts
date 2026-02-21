import { useState, useEffect, type RefObject } from 'react'
import { SLOT_WIDTH_PX, BANK_LEFT_OFFSET_PX } from '../constants'

/**
 * Observes a container element's content-box width and returns both the
 * integer slider count (for centering logic) and the pixel width (for smooth rendering).
 */
export function useVisibleSliderCount(containerRef: RefObject<HTMLDivElement | null>): { count: number; widthPx: number } {
  const [state, setState] = useState({ count: 12, widthPx: 12 * SLOT_WIDTH_PX })

  useEffect(() => {
    const el = containerRef.current
    if (!el) return

    const update = () => {
      const available = el.clientWidth - BANK_LEFT_OFFSET_PX
      const count = Math.max(1, Math.floor(available / SLOT_WIDTH_PX))
      setState({ count, widthPx: available })
    }

    update() // initial measurement

    const observer = new ResizeObserver(update)
    observer.observe(el)
    return () => observer.disconnect()
  }, [containerRef])

  return state
}

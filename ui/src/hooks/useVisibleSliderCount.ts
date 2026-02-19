import { useState, useEffect, type RefObject } from 'react'
import { SLOT_WIDTH_PX, BANK_PADDING_PX } from '../constants'

/**
 * Observes a container element's content-box width and computes how many
 * fixed-width slider slots fit inside it.
 */
export function useVisibleSliderCount(containerRef: RefObject<HTMLDivElement | null>): number {
  const [count, setCount] = useState(12)

  useEffect(() => {
    const el = containerRef.current
    if (!el) return

    const update = () => {
      const available = el.clientWidth - BANK_PADDING_PX
      setCount(Math.max(1, Math.floor(available / SLOT_WIDTH_PX)))
    }

    update() // initial measurement

    const observer = new ResizeObserver(update)
    observer.observe(el)
    return () => observer.disconnect()
  }, [containerRef])

  return count
}

"""Metro Design telemetry client (Python).

Usage:
    from platform.telemetry.python.telemetry import Telemetry

    tel = Telemetry(dsn="...", version="1.0.0")
    tel.info("plugin loaded")
    tel.counter("effect.apply", 1, {"effect": "film_look_01"})
    tel.exception(RuntimeError("test"))
"""

from __future__ import annotations

import atexit
import logging
import time
from dataclasses import dataclass, field
from threading import Lock
from typing import Dict, Optional


class _MetricsRegistry:
    """Minimal in-memory metric registry (Prometheus exposition stub)."""

    def __init__(self) -> None:
        self._lock = Lock()
        self._counters: Dict[str, float] = {}
        self._histograms: Dict[str, list[float]] = {}

    def inc(self, name: str, value: float = 1.0) -> None:
        with self._lock:
            self._counters[name] = self._counters.get(name, 0.0) + value

    def observe(self, name: str, value: float) -> None:
        with self._lock:
            self._histograms.setdefault(name, []).append(value)

    def snapshot(self) -> dict:
        with self._lock:
            return {
                "counters": dict(self._counters),
                "histograms": {k: list(v) for k, v in self._histograms.items()},
            }


class Telemetry:
    """Telemetry client for structured logging, metrics, and error reporting."""

    def __init__(
        self,
        dsn: str = "",
        version: str = "",
        enable_sentry: bool = False,
        enable_prometheus: bool = False,
    ) -> None:
        self._version = version
        self._registry = _MetricsRegistry()
        self._logger = logging.getLogger("metropolis.telemetry")
        self._logger.setLevel(logging.INFO)

        handler = logging.StreamHandler()
        handler.setFormatter(
            logging.Formatter(
                "[%(levelname)s] %(asctime)s %(message)s",
                datefmt="%Y-%m-%dT%H:%M:%S",
            )
        )
        self._logger.addHandler(handler)
        self._logger.info("Telemetry initialized (version=%s)", version)

        self._sentry_enabled = enable_sentry
        self._prometheus_enabled = enable_prometheus

        if enable_sentry:
            self._init_sentry(dsn)

        atexit.register(self.shutdown)

    def _init_sentry(self, dsn: str) -> None:
        try:
            import sentry_sdk  # type: ignore[import-untyped]

            sentry_sdk.init(dsn=dsn, release=self._version)
            self._logger.info("Sentry integration enabled")
        except ImportError:
            self._logger.warning("sentry-sdk not installed; sentry disabled")

    def debug(self, msg: str, **extra: object) -> None:
        self._logger.debug(msg, extra=extra)

    def info(self, msg: str, **extra: object) -> None:
        self._logger.info(msg, extra=extra)

    def warn(self, msg: str, **extra: object) -> None:
        self._logger.warning(msg, extra=extra)

    def error(self, msg: str, **extra: object) -> None:
        self._logger.error(msg, extra=extra)

    def counter(
        self,
        name: str,
        value: float = 1.0,
        labels: Optional[Dict[str, str]] = None,
    ) -> None:
        self._registry.inc(name, value)

    def histogram(self, name: str, value: float) -> None:
        self._registry.observe(name, value)

    def exception(self, exc: BaseException) -> None:
        self._logger.exception(str(exc))

    def metrics_snapshot(self) -> dict:
        return self._registry.snapshot()

    def shutdown(self) -> None:
        self._logger.info("Telemetry shutting down")
        if self._sentry_enabled:
            try:
                import sentry_sdk

                sentry_sdk.flush()
            except ImportError:
                pass

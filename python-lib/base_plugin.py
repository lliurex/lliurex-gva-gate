"""
Contrato que debe cumplir todo plugin.
Cada plugin hereda de BasePlugin e implementa 'name' y 'execute'.
"""

from abc import ABC, abstractmethod
from llxgvagate import User

class BasePlugin(ABC):

    @property
    @abstractmethod
    def name(self) -> str:
        """Palabra clave que activa este plugin (en minúsculas)."""
        ...

    @abstractmethod
    def autenticate(self, user, password, callback) -> User | None:
        """Lógica del plugin. Devuelve un string o None."""
        ...

    def __repr__(self) -> str:
        return f"<Plugin '{self.name}'>"

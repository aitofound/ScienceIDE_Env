from typing import IO

from numpy.typing import NDArray

def parse_n_doubles(file: IO[bytes], out: NDArray, nelem: int = -1) -> int: ...

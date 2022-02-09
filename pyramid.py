def pyramid(n: int) -> str:
    """
    Prints a pyramid made of asterics ('*') with the specified number of rows.
    >>> pyramid(3)
      *
     ***
    *****
    """
    STAR = '*'
    SPACE = ' '
    stri = ""

    for x in range(0,n):
        stri = STAR + (STAR * 2* x)
        print((SPACE * (n-x-1)) + stri)
/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

/* Split-recursive algorithm for computing factorial. This is used
   while computing the number of combination that will be required for
   the mapping table. Adapted from:
   http://www.luschny.de/math/factorial/binarysplitfact.html */
static int nminusnumofbits(int v) {
    long w = (long)v;
    w -= (0xaaaaaaaa & w) >> 1;
    w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
    w = w + (w >> 4) & 0x0f0f0f0f;
    w += w >> 8;
    w += w >> 16;
    return v - (int)(w & 0xff);
}
static long long partProduct(int n, int m) {
    if (m <= (n + 1)) return (long long) n;
    if (m == (n + 2)) return (long long) n * m; 
    int k =  (n + m) / 2;
    if ((k & 1) != 1) k = k - 1;
    return partProduct(n, k) * partProduct(k + 2, m);
}
static void loop(int n, long long *p, long long *r) {
    if (n <= 2) return;
    loop(n / 2, p, r);
    *p = *p * partProduct(n / 2 + 1 + ((n / 2) & 1), n - 1 + (n & 1));
    *r = *r * *p;
}
static long long Factorial(int n) {
    long long p = 1, r = 1;
    loop(n, &p, &r);
    return r << nminusnumofbits(n);
}

/* Compute C(n,r). */
long __eskel_Cnr(int n, int r) {
    return (Factorial(n)/(Factorial(n-r)*Factorial(r)));
}


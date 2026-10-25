// BENCH: list n=32768 reps=512
// Copy y Clone solo para poder construir el array fijo. No es un atributo
// de optimización: el bucle no pide inline ni unsafe.
#[derive(Copy, Clone)]
struct Node {
    val: i64,
    next: i64,
}

fn main() {
    let n: i64 = 32768;
    let reps: i64 = 512;
    let mut nodes = [Node { val: 0, next: 0 }; 32768];
    let last: i64 = n - 1;
    let mut i: i64 = 0;
    while i < last {
        nodes[i as usize].val = i;
        nodes[i as usize].next = i + 1;
        i = i + 1;
    }
    nodes[last as usize].val = last;
    nodes[last as usize].next = -1;
    let mut r: i64 = 0;
    let mut sum: i64 = 0;
    while r < reps {
        i = 0;
        while i >= 0 {
            sum = sum + nodes[i as usize].val;
            i = nodes[i as usize].next;
        }
        r = r + 1;
    }
    println!("{}", sum);
}

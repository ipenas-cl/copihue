(* BENCH: list n=32768 reps=512 *)
(* val en 2*i, next en 2*i+1. Un record de OCaml es un bloque con cabecera,
   no 16 bytes contiguos; el par vive en un array plano de int. La cadena
   de índices y el centinela -1 son los mismos. *)
let n = 32768
let reps = 512

let () =
  let nodes = Array.make (2 * n) 0 in
  let last = n - 1 in
  let i = ref 0 in
  while !i < last do
    nodes.(2 * !i) <- !i;
    nodes.(2 * !i + 1) <- !i + 1;
    i := !i + 1
  done;
  nodes.(2 * last) <- last;
  nodes.(2 * last + 1) <- -1;
  let r = ref 0 in
  let sum = ref 0 in
  while !r < reps do
    i := 0;
    while !i >= 0 do
      sum := !sum + nodes.(2 * !i);
      i := nodes.(2 * !i + 1)
    done;
    r := !r + 1
  done;
  Printf.printf "%d\n" !sum

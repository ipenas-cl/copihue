(* BENCH: arith n=100000000 *)
let n = 100000000

let () =
  let a = ref 1 in
  let b = ref 2 in
  let c = ref 3 in
  for i = 0 to n - 1 do
    a := (!a + !b) land 2147483647;
    b := (!b + !c) land 2147483647;
    c := (!c + i) land 2147483647
  done;
  Printf.printf "%d\n" (!a + !b + !c)

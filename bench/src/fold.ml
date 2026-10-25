(* BENCH: fold n=500000000 *)
let n = 500000000

let () =
  let s = ref 0 in
  for i = 0 to n - 1 do
    s := !s + i * 3 + 1
  done;
  Printf.printf "%d\n" !s

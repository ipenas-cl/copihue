(* BENCH: chain n=80000000 *)
let n = 80000000

let () =
  let x = ref 1 in
  for i = 0 to n - 1 do
    x := (!x + i) land 2147483647;
    x := (!x * 13) land 2147483647;
    x := !x lxor (!x asr 7)
  done;
  Printf.printf "%d\n" !x

(* BENCH: hash n=131072 reps=256 *)
let n = 131072
let reps = 256

let () =
  let buf = Bytes.make n '\000' in
  let i = ref 0 in
  while !i < n do
    Bytes.set buf !i (Char.chr (!i land 255));
    i := !i + 1
  done;
  let r = ref 0 in
  let total = ref 0 in
  while !r < reps do
    let h = ref 18652613 in
    i := 0;
    while !i < n do
      let b = Char.code (Bytes.get buf !i) in
      h := (!h * 16777619 + b) land 2147483647;
      i := !i + 1
    done;
    total := !total + !h;
    r := !r + 1
  done;
  Printf.printf "%d\n" !total

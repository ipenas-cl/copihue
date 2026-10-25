(* BENCH: fill n=262144 reps=128 *)
let n = 262144
let reps = 128

let () =
  let buf = Bytes.make n '\000' in
  let r = ref 0 in
  let sum = ref 0 in
  while !r < reps do
    let i = ref 0 in
    while !i < n do
      let b = Char.code (Bytes.get buf !i) in
      Bytes.set buf !i (Char.chr ((b + !i + !r) land 255));
      i := !i + 1
    done;
    i := 0;
    while !i < n do
      sum := !sum + Char.code (Bytes.get buf !i);
      i := !i + 1
    done;
    r := !r + 1
  done;
  Printf.printf "%d\n" !sum

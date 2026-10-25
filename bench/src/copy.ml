(* BENCH: copy n=262144 reps=192 *)
let n = 262144
let reps = 192

let () =
  let src = Bytes.make n '\000' in
  let dst = Bytes.make n '\000' in
  let i = ref 0 in
  while !i < n do
    Bytes.set src !i (Char.chr (!i land 255));
    i := !i + 1
  done;
  let r = ref 0 in
  let sum = ref 0 in
  while !r < reps do
    i := 0;
    while !i < n do
      Bytes.set dst !i (Bytes.get src !i);
      i := !i + 1
    done;
    i := 0;
    while !i < n do
      sum := !sum + Char.code (Bytes.get dst !i);
      let b = Char.code (Bytes.get src !i) in
      Bytes.set src !i (Char.chr ((b + 1) land 255));
      i := !i + 1
    done;
    r := !r + 1
  done;
  Printf.printf "%d\n" !sum

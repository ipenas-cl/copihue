(* BENCH: array n=100000 rounds=1000 *)
let n = 100000
let rounds = 1000

let () =
  let a = Array.make n 0 in
  for i = 0 to n - 1 do
    a.(i) <- (i * 3 + 1) land 1023
  done;
  for _r = 1 to rounds do
    for i = 0 to n - 1 do
      let v = a.(i) in
      a.(i) <- (v * 3 + (i land 7) + 1) land 1023
    done
  done;
  let sum = ref 0 in
  for i = 0 to n - 1 do
    sum := !sum + a.(i) * (i + 1)
  done;
  Printf.printf "%d\n" !sum

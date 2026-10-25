(* BENCH: matmul N=384 *)
let n = 384

let () =
  let a = Array.make (n * n) 0 in
  let b = Array.make (n * n) 0 in
  let c = Array.make (n * n) 0 in
  for i = 0 to n - 1 do
    for j = 0 to n - 1 do
      a.(i * n + j) <- (i + j) land 7;
      b.(i * n + j) <- (i * 3 + j) land 7;
      c.(i * n + j) <- 0
    done
  done;
  for i = 0 to n - 1 do
    for j = 0 to n - 1 do
      let s = ref 0 in
      for k = 0 to n - 1 do
        s := !s + a.(i * n + k) * b.(k * n + j)
      done;
      c.(i * n + j) <- !s
    done
  done;
  let sum = ref 0 in
  for i = 0 to n * n - 1 do
    sum := !sum + c.(i)
  done;
  Printf.printf "%d\n" !sum

// Ejemplo: Trabajo con arrays

fn main() -> i32 {
    // Declarar array
    let arr: [i32; 10];

    // Inicializar array
    let i = 0;
    while (i < 10) {
        arr[i] = i * 2;
        i++;
    }

    // Imprimir elementos
    println("Elementos del array:");
    let j = 0;
    while (j < 10) {
        print("  arr[");
        print_int(j);
        print("] = ");
        print_int(arr[j]);
        println("");
        j++;
    }

    // Calcular suma
    let suma = 0;
    let k = 0;
    while (k < 10) {
        suma += arr[k];
        k++;
    }

    print("Suma total: ");
    print_int(suma);
    println("");

    return 0;
}

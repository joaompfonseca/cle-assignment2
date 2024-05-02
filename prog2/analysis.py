import numpy as np

folder="results"
number_sizes=["32", "256K", "1M", "16M"]
n_procs=["1", "2", "4", "8"]

for size in number_sizes:
    for procs in n_procs:
        # Open the file and read lines
        with open(f"{folder}/n{size}-p{procs}.txt", 'r') as file:
            numbers = [float(line.strip()) for line in file]
        
        # Calculate the average
        average = np.mean(numbers)
        
        # Calculate the standard deviation
        standard_deviation = np.std(numbers)
        
        # Format the average and standard deviation in scientific notation with 3 decimal digits
        formatted_average = "{:.3e}".format(average)
        formatted_standard_deviation = "{:.3e}".format(standard_deviation)
        
        # Print the results
        print(f"Numbers: {size} - Processes: {procs}")
        print(f"Average: {formatted_average}")
        print(f"Standard Deviation: {formatted_standard_deviation}")
        print()
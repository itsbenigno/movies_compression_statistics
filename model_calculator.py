import pmdarima as pm
import pandas as pd
import sys
import csv

"""Takes a timeseries filename as input, and the max (p,d,q) to check for modeling the 
timeseries. It reads the timeseries, and return the bests (p,d,q)"""
def compute_pdq(timeseries_in, max_p_in, max_d_in, max_q_in):
    timeseries = pd.read_csv(timeseries_in)
    if timeseries.isnull().values.any():
        raise ValueError("Error: The time series data contains null values.")
    model = pm.auto_arima(timeseries, max_p=max_p_in, max_d=max_d_in, max_q=max_q_in, start_p=0,
                          start_q=0, seasonal=False, information_criterion='aic', error_action='ignore')
    p, d, q = model.order
    return p, d, q


"""Check number and type of arguments"""
def parse_input(argv):
    if len(argv) != 5:
        raise ValueError(
            "Error: 4 arguments are required - a string followed by 3 positive integers.")

    string_value = argv[1]
    try:
        int_values = [int(val) for val in argv[2:]]
    except ValueError:
        raise ValueError(
            "Error: the 3 arguments after the string should be integers.")

    if all(val > 0 for val in int_values):
        return string_value, int_values
    else:
        raise ValueError(
            "Error: the 3 arguments after the string should be positive integers.")


def main():
    try:
        input_params = parse_input(sys.argv)
        timeseries = input_params[0]
        max_p_in, max_d_in, max_q_in = input_params[1]
        p, d, q = compute_pdq(timeseries, max_p_in, max_d_in,
                              max_q_in)  # order of the series

        # write order on csv
        rows = [[p, d, q]]
        csv_name = "best_ARIMA_order.csv"
        with open(csv_name, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerows(rows)
    except ValueError as error:
        print(error, file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()

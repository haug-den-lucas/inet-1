import math
from omnetpp.scave import results, chart, utils, ideplot
import matplotlib.pyplot as plt
import pandas as pd
import ast
import re

debug = False

def plot_vectors(df, props, legend_func=utils.make_legend_label):
    """
    Creates a line plot from the dataframe, with styling and additional input
    coming from the properties. Each row in the dataframe defines a series.

    Colors and markers are assigned automatically. The `cycle_seed` property
    allows you to select other combinations if the default one is not suitable.

    A function to produce the legend labels can be passed in. By default,
    `make_legend_label()` is used, which offers many ways to influence the
    legend via datataframe columns and chart properties. In the absence of
    more specified settings, the legend is normally computed from columns which best
    differentiate among the vectors.

    Parameters:

    - `df`: the dataframe
    - `props` (dict): the properties
    - `legend_func`: the function to produce custom legend labels

    Columns of the dataframe:

    - `vectime`, `vecvalue` (Numpy `ndarray`'s of matching sizes): the x and y coordinates for the plot
    - `interpolationmode` (str, optional): this column normally comes from a result attribute, and determines how the points will be connected
    - `legend` (optional): legend label for the series; if missing, legend labels are derived from other columns
    - `name`, `title`, `module`, etc. (optional): provide input for the legend

    Notable properties that affect the plot:

    - `title`: plot title (autocomputed if missing)
    - `legend_labels`: selects whether to prefer the `name` or the `title` column for the legend
    - `drawstyle`: Matplotlib draw style; if present, it overrides the draw style derived from `interpolationmode`.
    - `linestyle`, `linecolor`, `linewidth`, `marker`, `markersize`: styling
    - `cycle_seed`: Alters the sequence in which colors and markers are assigned to series.
    """
    p = ideplot if chart.is_native_chart() else plt

    def get_prop(k):
        return props[k] if k in props else None

    title_cols, legend_cols = utils.extract_label_columns(df, props)
    
    if 'order' in df.columns:
        df.sort_values(by='order', inplace=True)
    else:
        df.sort_values(by=legend_cols, inplace=True)
    for t in df.itertuples(index=False):
        style = utils._make_line_args(props, t, df)
#        if t.propertyname != '':
#            style[t.propertyname] = t.propertyvalue
        style_dict = eval(t.additional_style)
        # print("style_dict:", style_dict)
        for i in style_dict.items():
            style[i[0]] = i[1]
        # print("style:", style)
        p.plot(t.vectime, t.vecvalue, label=legend_func(legend_cols, t, props), **style)

    title = get_prop("title") or utils.make_chart_title(df, title_cols)
    utils.set_plot_title(title)

    p.ylabel(utils.make_chart_title(df, ["title"]))
    
def plot_vectors_separate_grouped(df_list, props, legend_func=utils.make_legend_label):
    p = ideplot if chart.is_native_chart() else plt
    
    title = ""
    ax_list = []
    for j in range(0, len(df_list)):
        df = df_list[j]

        def get_prop(k):
            return props[k] if k in props else None
    
        title_cols, legend_cols = utils.extract_label_columns(df, props)
        
        ax = None
        ax = plt.subplot(len(df_list), 1, j+1, sharex=ax)
    
        if 'order' in df.columns:
            df.sort_values(by='order', inplace=True)
        else:
            df.sort_values(by=legend_cols, inplace=True)
            
        for t in df.itertuples(index=False):
            style = utils._make_line_args(props, t, df)
            style_dict = eval(t.additional_style)
            for i in style_dict.items():
                style[i[0]] = i[1]
            p.plot(t.vectime, t.vecvalue, label=legend_func(legend_cols, t, props), **style)
        
        if j == 0:
            title = get_prop("title") or utils.make_chart_title(df, title_cols)

            #utils.set_plot_title(title)
            plt.suptitle(title)
        ax_list.append(ax)

    p.ylabel(utils.make_chart_title(df, ["title"]))
    
    return ax_list

def add_to_dataframe(df, style_tuple_list, default_dict={}, order={}):
    """
    Adds 'additional_style' column to dataframe. The concent of this column is added to 'style' object when plotting.
    Can also specify row order in dataframe (e.g. for ordering items in legend).
    style_tuple_list: [(column, value, {style dictionary}), (...), ...]:
        add style dictionary concents to 'additional_style' column in rows where column=value
    default_dict: {style dictionary}:
        add style to rows not matched by the above
    order: {'configname': order, ...}
        2-member list: first one is the column in the dataframe to order by, second is a dict with the order numbers
        
    example:
    style_tuple_list = [('legend', 'eth[0]', {'linestyle': '--', 'linewidth': 2}), ('legend', 'eth[1]', {'linestyle': '-', 'linewidth': 2, 'marker': 's', 'markersize': 4})]
    default_dict = {'linestyle': '-', 'linewidth': 1}
    order = ['configname', {'Default_config': 1, 'Advanced_config': 0, 'Manual_config': 2}]
    
    Note that the value parameter in style_tuple_list can contain regex (e.g. .*foo). Make sure to escape regex characters such as [ and ] with \
    """
    
    df['additional_style'] = None
    
    def add_separator():
        print("")
    
    for i in style_tuple_list:
        column = i[0]
        value = i[1]
        style_tuple = i[2]
        
        for i in range(0,len(df)):
            pattern = re.compile(value)
            match = re.fullmatch(pattern, df[column][i])
            if match != None:
                if debug:
                    print("MATCH FOUND:")
                    print("    column:", column)
                    print("    value:", value)
                    print("    match:", match.group())
                if (df['additional_style'][i] == None):
                    df['additional_style'][i] = str(style_tuple)
                    if debug: print("Adding style tuple:", str(style_tuple))
                    if debug: add_separator()
                else:
                    orig_style_dict = ast.literal_eval(df['additional_style'][i])       # convert already added style to dict
                    if debug: print("Adding style tuple to existing style dict", orig_style_dict, type(orig_style_dict))
                    orig_style_dict.update(style_tuple)                                 # add new style to dict
                    if debug: print("New style dict", orig_style_dict, type(orig_style_dict))
                    if debug: add_separator()
                    df['additional_style'][i] = str(orig_style_dict)                    # add to dataframe as string
                
    for i in range(0,len(df)):
        if (df['additional_style'][i] == None):
            if debug: print('adding default stuff')
            df['additional_style'][i] = str(default_dict)
            
    if order:
        # order the dataframe
        df['order'] = None
        
        order_column = order[0]
        order_dict = order[1]
        for i in order_dict.items():
            for j in range(0,len(df)):
                if df[order_column][j] == i[0]:
                    df['order'][j] = i[1]
                    
        if debug: print("order added.", df['order'], df)

    return df
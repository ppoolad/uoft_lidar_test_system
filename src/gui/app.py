import streamlit as st
st.set_page_config(page_title="Lidar Test System", page_icon="📡", layout="wide")

home_page = st.Page("pages/home.py", title="Home", icon="📡", default=True)
data_analysis_page = st.Page("pages/data_analysis.py", title="Data Analysis", icon="📊")

pg = st.navigation([home_page, data_analysis_page])
pg.run()
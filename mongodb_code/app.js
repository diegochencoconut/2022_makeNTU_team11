const express = require('express');
const mongoose = require('mongoose');
const app = express()
require('dotenv/config');

const evaluationRoute = require('./routes/evaluation')
const addMealRoute = require('./routes/add_meal')
const doorRoute = require('./routes/door')
app.use('/evaluation', evaluationRoute)
app.use('/add_meal', addMealRoute)
app.use('/door', doorRoute)

app.get('/', (req, res) => {
    res.send("Connect successfully!")
})

mongoose.connect(
    process.env.DB_CONNECTION,
    console.log('connected to DB!')
)

app.listen(3000);

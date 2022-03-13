const mongoose = require('mongoose');

const AddmealSchema = mongoose.Schema({
    type:
    {
        type: String,
        default: "Add_meal",
    },
    room:
    {
        type: Number,
        required: true,
    },
    order:
    {
        type: String,
        required: true,
    },
    date:
    {
        type: Date,
        default: Date.now,
    },

})

module.exports = mongoose.model('Add_meal', AddmealSchema);
const express = require('express');
const router = express.Router();
const Post = require('../models/Door');
const bodyParser = require('body-parser')
const jsonParser = bodyParser.json()

//for line notify
const axios = require('axios')

const webhook_url = 'https://notify-api.line.me/api/notify'
const oauthToken = '0JrO6qg1us2orgoBQBisrtB28mh9SE3OCoKYVSLRMlf'

router.get('/', async (req, res) => {
    try
    {
        const test = await Post.findOne(
                { $or:[
                   {door: "open"},
                   {door: "request"},
                ]}
        )
        if (test == null)
        {
            res.json({"result": "accepted"});
        }
        else
        {
            res.json({"result": "rejected"});
        }
    }
    catch(err)
    {
        res.json({message:err});
    }
});

router.post('/', jsonParser, async (req, res) => {
    const post = new Post({
        room: req.body.room,
        door: req.body.door,
    });
    try
    {
        const test = await Post.findOne(
            {room: req.body.room}
        )

        if (test == null)
        {
            const savedPost = await post.save();
            console.log(savedPost);
            res.json({"message": "The room is created"});
        }
        else
        {
            res.json({"message": "Room existed"})
        }
    }
    catch(err)
    {
        res.json({message: err})
    }
});

router.patch('/:room', jsonParser, async(req, res)=>{
    try
    {
        if (req.body.door == "request")
        {
            const test = await Post.findOne(
                { $or:[
                   {door: "open"},
                   {door: "request"},
                ]}
            )
            if (test == null)
            {
                const updatedPost = await Post.updateOne(
                    {
                        type: "Door",
                        room: req.params.room
                    },
                    {$set: {door: "request"}}
                );
                res.json({"message": "accepted"});
            }
            else
            {
                res.json({"message": "rejected"});
            }
        }
        else if (req.body.door == "open")
        {
            const test = await Post.findOne(
                { $and:[
                    {room: req.params.room},
                    {door: "request"},
                 ]}
            )
            if (test == null)   //open without allow
            {             
                //Post to line server
                const data = new URLSearchParams();
                var content = `Room ${req.params.room} open the door without permission!`
                data.append('message', content);
                axios
                .post(webhook_url, data, {
                    headers: {
                    'content-type': 'application/x-www-form-urlencoded;charset=utf-8',
                    'Authorization': 'Bearer ' + oauthToken
                }})
                .catch(error => {
                    console.error(error)
                })
            }
            const updatedPost = await Post.updateOne(
                {
                    type: "Door",
                    room: req.params.room
                },
                {$set: {door: req.body.door}}
            );
            res.json({"message": "success"});
        }
        else
        {
            const updatedPost = await Post.updateOne(
                {
                    type: "Door",
                    room: req.params.room
                },
                {$set: {door: req.body.door}}
            );
            res.json({"message": "success"});
        }
    }
    catch(err)
    {
        res.json({message: err})
    }
})


module.exports = router;